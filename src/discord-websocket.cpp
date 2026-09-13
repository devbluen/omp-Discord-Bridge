/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#include "discord-websocket.hpp"
#include "discord-log.hpp"
#include "discord-json.hpp"
#include "discord-tls.hpp"
#include "utils.hpp"
#include "version.hpp"
#include <core.hpp>
#include <openssl/ssl.h>
#include <algorithm>
#include <exception>

namespace
{
const char* gatewayPlatform() noexcept
{
#if defined(_WIN32)
	return "windows";
#elif defined(__linux__)
	return "linux";
#else
	return "unknown";
#endif
}
}

void DiscordWebSocket::logGatewayError(const char* stage, const beast::error_code& ec)
{
	if (!ec) return;
	const std::string key = std::string(stage) + ":" + ec.message();
	const auto now = std::chrono::steady_clock::now();
	if (key == lastGatewayErrorKey_ && now - lastGatewayErrorAt_ < std::chrono::minutes(1)) return;
	lastGatewayErrorKey_ = key;
	lastGatewayErrorAt_ = now;
	DiscordLogWarning(core_, std::string("[DiscordBridge] Gateway ") + stage + " failed: " + ec.message());
}

bool DiscordWebSocket::reportFatalClose(int code)
{
	std::string message;
	switch (code)
	{
		case 4004:
			message = "the bot token is invalid (close code 4004). Reset the token in the Discord Developer Portal and update it";
			break;
		case 4012:
			message = "the gateway API version is not supported (close code 4012)";
			break;
		case 4013:
			message = "the intents value is invalid (close code 4013). Build it from DISCORD_INTENT_* flags";
			break;
		case 4014:
		{
			std::string missing;
			const auto add = [&missing, this](int bit, const char* name)
			{
				if (intents_ & bit)
				{
					if (!missing.empty()) missing += ", ";
					missing += name;
				}
			};
			add(1 << 1, "Server Members");
			add(1 << 8, "Presence");
			add(1 << 15, "Message Content");
			message = "the bot requested privileged intents that are not enabled (close code 4014). Enable " +
				(missing.empty() ? std::string("the privileged intents") : missing) +
				" in the Discord Developer Portal (Bot > Privileged Gateway Intents), or connect with DISCORD_INTENTS_DEFAULT";
			break;
		}
		default:
			return false;
	}
	DiscordLogWarning(core_, "[DiscordBridge] Discord refused the connection: " + message + ".");
	return true;
}

DiscordWebSocket::DiscordWebSocket(DiscordBot* bot, ICore* core, const std::string& token, int intents)
	: sslCtx_(ssl::context::tlsv12_client)
	, resolver_(ioc_)
	, botToken_(token)
	, intents_(intents)
	, heartbeatInterval_(0)
	, lastSequence_(-1)
	, gatewayHost_(GATEWAY_HOST)
	, gatewayPort_(GATEWAY_PORT)
	, gatewayPath_(GATEWAY_PATH)
	, initialGatewayHost_(GATEWAY_HOST)
	, initialGatewayPort_(GATEWAY_PORT)
	, initialGatewayPath_(GATEWAY_PATH)
	, connected_(false)
	, identified_(false)
	, shouldStop_(false)
	, bot_(bot)
	, core_(core)
	, heartbeatTimer_(ioc_)
	, reconnectTimer_(ioc_)
{
	DiscordTLS::configureCertificateVerification(sslCtx_);
	sslCtx_.set_verify_mode(ssl::verify_peer);
	sslCtx_.set_verify_callback(ssl::host_name_verification(gatewayHost_));
}

DiscordWebSocket::~DiscordWebSocket()
{
	disconnect();
}

bool DiscordWebSocket::connect()
{
	if (networkThread_.joinable())
	{
		return !shouldStop_.load();
	}

	shouldStop_ = false;
	connected_ = false;
	identified_ = false;
	reconnectAttempt_ = 0;
	reconnectScheduled_ = false;
	ioc_.restart();
	networkThread_ = std::thread(&DiscordWebSocket::run, this);
	return true;
}

void DiscordWebSocket::run()
{
	{
		std::lock_guard<std::mutex> lock(networkStateMutex_);
		networkStarted_ = true;
		networkRunning_ = true;
	}
	networkStateCondition_.notify_all();
	startResolve();
	ioc_.run();
	{
		std::lock_guard<std::mutex> lock(networkStateMutex_);
		networkRunning_ = false;
	}
	networkStateCondition_.notify_all();
}

void DiscordWebSocket::startResolve()
{
	if (shouldStop_)
	{
		return;
	}

	resolver_.async_resolve(gatewayHost_, gatewayPort_,
		[this](beast::error_code ec, tcp::resolver::results_type results)
		{
			onResolve(ec, std::move(results));
		});
}

void DiscordWebSocket::onResolve(beast::error_code ec, tcp::resolver::results_type results)
{
	if (shouldStop_)
	{
		return;
	}
	if (ec)
	{
		logGatewayError("DNS resolution", ec);
		scheduleReconnect();
		return;
	}

	auto connection = std::make_shared<WebSocketStream>(ioc_, sslCtx_);
	ws_ = connection;
	beast::get_lowest_layer(*connection).expires_after(std::chrono::seconds(30));
	beast::get_lowest_layer(*connection).async_connect(results,
		[this, connection](beast::error_code connectEc, tcp::resolver::results_type::endpoint_type endpoint)
		{
			onConnect(connectEc, endpoint, connection);
		});
}

void DiscordWebSocket::onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type, std::shared_ptr<WebSocketStream> connection)
{
	if (shouldStop_ || ws_ != connection)
	{
		return;
	}
	if (ec)
	{
		logGatewayError("TCP connect", ec);
		abandonConnection(connection);
		scheduleReconnect();
		return;
	}

	if (!SSL_set_tlsext_host_name(connection->next_layer().native_handle(), gatewayHost_.c_str()))
	{
		DiscordLogWarning(core_, "[DiscordBridge] Gateway TLS hostname setup failed");
		abandonConnection(connection);
		scheduleReconnect();
		return;
	}
	connection->next_layer().async_handshake(ssl::stream_base::client,
		[this, connection](beast::error_code handshakeEc)
		{
			onSslHandshake(handshakeEc, connection);
		});
}

void DiscordWebSocket::onSslHandshake(beast::error_code ec, std::shared_ptr<WebSocketStream> connection)
{
	if (shouldStop_ || ws_ != connection)
	{
		return;
	}
	if (ec)
	{
		logGatewayError("TLS handshake", ec);
		abandonConnection(connection);
		scheduleReconnect();
		return;
	}

	beast::get_lowest_layer(*connection).expires_never();
	connection->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
	connection->set_option(websocket::stream_base::decorator([](websocket::request_type& request)
	{
		request.set(http::field::user_agent, std::string("discord-bridge/") + DISCORD_BRIDGE_VERSION + " (open.mp)");
	}));
	connection->async_handshake(gatewayHost_, gatewayPath_,
		[this, connection](beast::error_code handshakeEc)
		{
			onHandshake(handshakeEc, connection);
		});
}

void DiscordWebSocket::onHandshake(beast::error_code ec, std::shared_ptr<WebSocketStream> connection)
{
	if (shouldStop_ || ws_ != connection)
	{
		return;
	}
	if (ec)
	{
		logGatewayError("WebSocket handshake", ec);
		abandonConnection(connection);
		scheduleReconnect();
		return;
	}

	connection->read_message_max(MAX_GATEWAY_MESSAGE_BYTES);
	reconnectAttempt_ = 0;
	reconnectScheduled_ = false;
	writeNext(connection);
	readNext(connection, std::make_shared<beast::flat_buffer>());
}

void DiscordWebSocket::readNext(std::shared_ptr<WebSocketStream> connection, std::shared_ptr<beast::flat_buffer> readBuffer)
{
	if (shouldStop_ || ws_ != connection)
	{
		return;
	}
	connection->async_read(*readBuffer,
		[this, connection, readBuffer](beast::error_code ec, std::size_t bytesTransferred)
		{
			onRead(ec, bytesTransferred, connection, readBuffer);
		});
}

void DiscordWebSocket::onRead(beast::error_code ec, std::size_t, std::shared_ptr<WebSocketStream> connection, std::shared_ptr<beast::flat_buffer> readBuffer)
{
	if (shouldStop_ || ws_ != connection)
	{
		return;
	}
	if (ec)
	{
		// Some close codes mean reconnecting can never succeed (bad token,
		// intents that are not enabled...).  Explain them and stop retrying.
		if (ec == websocket::error::closed && reportFatalClose(static_cast<int>(connection->reason().code)))
		{
			abandonConnection(connection);
			return;
		}
		if (!shouldStop_) logGatewayError("read", ec);
		abandonConnection(connection);
		if (!shouldStop_)
		{
			scheduleReconnect();
		}
		return;
	}

	connected_ = true;
	const std::string message = beast::buffers_to_string(readBuffer->data());
	readBuffer->consume(readBuffer->size());
	handleMessage(message);
	readNext(connection, std::move(readBuffer));
}

void DiscordWebSocket::scheduleReconnect()
{
	if (shouldStop_)
	{
		return;
	}
	if (reconnectScheduled_)
	{
		return;
	}
	reconnectScheduled_ = true;
	const unsigned int seconds = std::min(60U, reconnectAttempt_ == 0 ? 0U : (1U << std::min(reconnectAttempt_, 6U)));
	++reconnectAttempt_;
	reconnectTimer_.expires_after(std::chrono::seconds(seconds));
	reconnectTimer_.async_wait([this](beast::error_code ec)
	{
		onReconnect(ec);
	});
}

void DiscordWebSocket::onReconnect(beast::error_code ec)
{
	if (ec || shouldStop_)
	{
		return;
	}
	reconnectScheduled_ = false;
	startResolve();
}

void DiscordWebSocket::scheduleHeartbeat()
{
	if (shouldStop_ || heartbeatInterval_ <= 0)
	{
		return;
	}
	heartbeatTimer_.expires_after(std::chrono::milliseconds(heartbeatInterval_));
	heartbeatTimer_.async_wait([this](beast::error_code ec)
	{
		onHeartbeat(ec);
	});
}

void DiscordWebSocket::onHeartbeat(beast::error_code ec)
{
	if (ec || shouldStop_)
	{
		return;
	}
	sendHeartbeat();
	scheduleHeartbeat();
}

void DiscordWebSocket::disconnect()
{
	shouldStop_ = true;
	connected_ = false;
	identified_ = false;

	// All Beast streams, timers, and the resolver belong to the io_context
	// thread.  Post their shutdown there and join only after that handler has
	// made the transport inert; touching them directly races callbacks.
	if (networkThread_.joinable())
	{
		bool networkRunning = false;
		{
			std::unique_lock<std::mutex> lock(networkStateMutex_);
			networkStateCondition_.wait(lock, [this]() { return networkStarted_; });
			networkRunning = networkRunning_;
		}

		bool shutdownPosted = false;
		if (networkRunning)
		{
			try
			{
				net::post(ioc_, [this]() { shutdownOnIoThread(); });
				shutdownPosted = true;
			}
			catch (const std::exception&)
			{
				ioc_.stop();
			}
		}
		else
		{
			ioc_.stop();
		}
		networkThread_.join();
		if (!shutdownPosted) shutdownOnIoThread();
		{
			std::lock_guard<std::mutex> lock(networkStateMutex_);
			networkStarted_ = false;
		}
	}
	else
	{
		shutdownOnIoThread();
	}

	// Release canceled handlers while the owning object is still fully alive.
	ioc_.restart();
	ioc_.poll();
	ioc_.stop();
}

void DiscordWebSocket::shutdownOnIoThread()
{
	heartbeatTimer_.cancel();
	reconnectTimer_.cancel();
	resolver_.cancel();
	if (ws_)
	{
		beast::error_code closeEc;
		auto& socket = beast::get_lowest_layer(*ws_).socket();
		socket.cancel(closeEc);
		socket.close(closeEc);
		ws_.reset();
	}

	if (!sendQueue_.empty())
	{
		pendingSendCount_.fetch_sub(sendQueue_.size(), std::memory_order_relaxed);
		pendingSendBytes_.fetch_sub(sendQueueBytes_, std::memory_order_relaxed);
	}
	sendQueue_.clear();
	sendQueueBytes_ = 0;
	sendQueueLimitLogged_ = false;
	writeInProgress_ = false;
	reconnectScheduled_ = false;
	ioc_.stop();
}

void DiscordWebSocket::queueWrite(std::string message)
{
	if (shouldStop_ || message.empty())
	{
		if (!message.empty())
		{
			pendingSendCount_.fetch_sub(1, std::memory_order_relaxed);
			pendingSendBytes_.fetch_sub(message.size(), std::memory_order_relaxed);
		}
		return;
	}
	if (sendQueue_.size() >= MAX_SEND_QUEUE_MESSAGES || sendQueueBytes_ > MAX_SEND_QUEUE_BYTES - message.size())
	{
		if (!sendQueueLimitLogged_)
		{
			DiscordLogWarning(core_, "[DiscordBridge] Gateway outbound queue limit reached; message dropped");
			sendQueueLimitLogged_ = true;
		}
		pendingSendCount_.fetch_sub(1, std::memory_order_relaxed);
		pendingSendBytes_.fetch_sub(message.size(), std::memory_order_relaxed);
		return;
	}
	sendQueueBytes_ += message.size();
	if (sendQueue_.size() < MAX_SEND_QUEUE_MESSAGES / 2 && sendQueueBytes_ < MAX_SEND_QUEUE_BYTES / 2)
	{
		sendQueueLimitLogged_ = false;
	}
	sendQueue_.push_back(std::make_shared<std::string>(std::move(message)));
	writeNext(ws_);
}

void DiscordWebSocket::writeNext(std::shared_ptr<WebSocketStream> connection)
{
	if (!connection || ws_ != connection || !connected_ || writeInProgress_ || sendQueue_.empty() || shouldStop_)
	{
		return;
	}
	writeInProgress_ = true;
	const auto message = sendQueue_.front();
	connection->async_write(net::buffer(*message),
		[this, connection, message](beast::error_code ec, std::size_t bytesTransferred)
		{
			onWrite(ec, bytesTransferred, connection, message);
		});
}

void DiscordWebSocket::onWrite(beast::error_code ec, std::size_t, std::shared_ptr<WebSocketStream> connection, std::shared_ptr<std::string> message)
{
	if (shouldStop_ || ws_ != connection)
	{
		return;
	}
	writeInProgress_ = false;
	if (!sendQueue_.empty() && sendQueue_.front() == message)
	{
		const std::size_t messageSize = message ? message->size() : 0;
		sendQueue_.pop_front();
		sendQueueBytes_ -= std::min(sendQueueBytes_, messageSize);
		pendingSendCount_.fetch_sub(1, std::memory_order_relaxed);
		pendingSendBytes_.fetch_sub(messageSize, std::memory_order_relaxed);
	}
	if (ec)
	{
		abandonConnection(connection);
		if (!shouldStop_) scheduleReconnect();
		return;
	}
	writeNext(connection);
}

void DiscordWebSocket::abandonConnection(std::shared_ptr<WebSocketStream> connection)
{
	if (!connection || ws_ != connection)
	{
		return;
	}

	connected_ = false;
	identified_ = false;
	writeInProgress_ = false;
	if (!sendQueue_.empty())
	{
		pendingSendCount_.fetch_sub(sendQueue_.size(), std::memory_order_relaxed);
		pendingSendBytes_.fetch_sub(sendQueueBytes_, std::memory_order_relaxed);
	}
	sendQueue_.clear();
	sendQueueBytes_ = 0;
	sendQueueLimitLogged_ = false;

	// Cancel/close the transport instead of synchronously shutting down TLS
	// while another Beast operation may still be using the stream.  The
	// shared_ptr captured by each handler keeps the stream alive until the
	// canceled operation has finished, and the identity check above ignores
	// callbacks from an obsolete connection after a reconnect.
	beast::error_code closeEc;
	auto& socket = beast::get_lowest_layer(*connection).socket();
	socket.cancel(closeEc);
	socket.close(closeEc);
	ws_.reset();
}

bool DiscordWebSocket::sendMessage(const std::string& message)
{
	if (shouldStop_ || message.empty() || message.size() > 4096)
	{
		return false;
	}

	const std::size_t messageSize = message.size();
	std::size_t count = pendingSendCount_.load(std::memory_order_relaxed);
	while (count < MAX_SEND_QUEUE_MESSAGES && !pendingSendCount_.compare_exchange_weak(
		count, count + 1, std::memory_order_relaxed, std::memory_order_relaxed))
	{
	}
	if (count >= MAX_SEND_QUEUE_MESSAGES)
	{
		return false;
	}

	std::size_t bytes = pendingSendBytes_.load(std::memory_order_relaxed);
	while (bytes <= MAX_SEND_QUEUE_BYTES - messageSize
		&& !pendingSendBytes_.compare_exchange_weak(bytes, bytes + messageSize,
			std::memory_order_relaxed, std::memory_order_relaxed))
	{
	}
	if (bytes > MAX_SEND_QUEUE_BYTES - messageSize)
	{
		pendingSendCount_.fetch_sub(1, std::memory_order_relaxed);
		return false;
	}

	try
	{
		net::post(ioc_, [this, message]
		{
			queueWrite(message);
		});
	}
	catch (const std::exception&)
	{
		pendingSendCount_.fetch_sub(1, std::memory_order_relaxed);
		pendingSendBytes_.fetch_sub(messageSize, std::memory_order_relaxed);
		return false;
	}
	return true;
}

bool DiscordWebSocket::sendPresenceUpdate(int status, int activityType, const std::string& activityName, const std::string& activityUrl)
{
	if (shouldStop_)
	{
		return false;
	}

	DiscordJson payload {
		{ "op", 3 },
		{ "d", {
			{ "since", nullptr },
			{ "activities", DiscordJson::array() },
			{ "status", status == 1 ? "dnd" : status == 2 ? "idle" : status == 3 ? "invisible" : status == 4 ? "offline" : "online" },
			{ "afk", false }
		} }
	};
	if (!activityName.empty())
	{
		DiscordJson activity = { { "name", activityName }, { "type", activityType } };
		// Custom statuses display `state`; Discord still requires a name.
		if (activityType == 4) activity = { { "name", "Custom Status" }, { "type", 4 }, { "state", activityName } };
		if (activityType == 1 && !activityUrl.empty()) activity["url"] = activityUrl;
		payload["d"]["activities"].push_back(std::move(activity));
	}
	return sendMessage(payload.dump());
}

void DiscordWebSocket::requestGuildMembers(const std::string& guildId)
{
	DiscordJson payload = {
		{ "op", 8 },
		{ "d", { { "guild_id", guildId }, { "query", "" }, { "limit", 0 } } }
	};
	sendMessage(payload.dump());
}

void DiscordWebSocket::handleMessage(const std::string& message)
{
	DiscordJson payload = DiscordJson::parse(message, nullptr, false);
	if (payload.is_discarded() || !payload.is_object())
	{
		return;
	}

	const int op = payload.value("op", -1);
	bool payloadWasRewritten = false;
	if ((payload.find("s") != payload.end()) && !payload["s"].is_null() && payload["s"].is_number_integer())
	{
		lastSequence_ = payload["s"].get<int64_t>();
	}

	if (op == 0)
	{
		const std::string event = payload.value("t", std::string());
		if (event == "READY" && (payload.find("d") != payload.end()) && payload["d"].is_object())
		{
			sessionId_ = payload["d"].value("session_id", std::string());
			resumeGatewayUrl_ = payload["d"].value("resume_gateway_url", std::string());
			if (!resumeGatewayUrl_.empty()) setGatewayUrl(resumeGatewayUrl_);
		}
		if ((event == "GUILD_CREATE" || event == "GUILD_UPDATE") && payload["d"].is_object())
		{
			// Keep the small snapshots Discord already gives us.  Large guild
			// snapshots are still bounded before they reach the server tick;
			// their members are rehydrated through GUILD_MEMBERS_CHUNK instead.
			constexpr size_t MAX_INLINE_COLLECTION = 100;
			if (payload["d"].find("members") != payload["d"].end()
				&& payload["d"]["members"].is_array()
				&& payload["d"]["members"].size() > MAX_INLINE_COLLECTION)
			{
				payload["d"].erase("members");
				payloadWasRewritten = true;
			}
			if (payload["d"].find("presences") != payload["d"].end()
				&& payload["d"]["presences"].is_array()
				&& payload["d"]["presences"].size() > MAX_INLINE_COLLECTION)
			{
				payload["d"].erase("presences");
				payloadWasRewritten = true;
			}
			if (payload["d"].find("voice_states") != payload["d"].end()
				&& payload["d"]["voice_states"].is_array()
				&& payload["d"]["voice_states"].size() > MAX_INLINE_COLLECTION)
			{
				payload["d"].erase("voice_states");
				payloadWasRewritten = true;
			}
		}
		if (event == "GUILD_MEMBERS_CHUNK" && payload["d"].is_object() && payload["d"]["members"].is_array() && payload["d"]["members"].size() > 100)
		{
			// Discord can return up to a thousand members in one chunk.  Split
			// it before handing it to the component so each server tick only
			// materializes a small bounded batch of users.
			const DiscordJson members = payload["d"]["members"];
			for (size_t offset = 0; offset < members.size(); offset += 100)
			{
				DiscordJson part = payload;
				part["d"]["members"] = DiscordJson::array();
				const size_t end = std::min(offset + 100, members.size());
				for (size_t index = offset; index < end; ++index) part["d"]["members"].push_back(members[index]);
				if (messageCallback_) messageCallback_(part.dump());
			}
			return;
		}
	}
	else if (op == 1)
	{
		sendHeartbeat();
	}
	else if (op == 7)
	{
		abandonConnection(ws_);
		scheduleReconnect();
		return;
	}
	else if (op == 9)
	{
		const bool resumable = (payload.find("d") != payload.end()) && payload["d"].is_boolean() && payload["d"].get<bool>();
		identified_ = false;
		if (!resumable)
		{
			sessionId_.clear();
			lastSequence_ = -1;
			restoreInitialGatewayUrl();
		}
		abandonConnection(ws_);
		// Discord asks clients to wait briefly before re-identifying after an
		// invalid session.  Starting the backoff at two seconds satisfies that
		// requirement while still allowing the normal reconnect path to reset
		// its attempt counter after a successful handshake.
		reconnectAttempt_ = std::max(1U, reconnectAttempt_);
		scheduleReconnect();
		return;
	}
	else if (op == 10 && (payload.find("d") != payload.end()) && payload["d"].is_object())
	{
		heartbeatInterval_ = payload["d"].value("heartbeat_interval", 0);
		heartbeatTimer_.cancel();
		scheduleHeartbeat();
		if (!identified_)
		{
			if (!sessionId_.empty() && lastSequence_ >= 0)
			{
				sendResume();
			}
			else
			{
				sendIdentify();
			}
			identified_ = true;
		}
	}

	if (messageCallback_)
	{
		messageCallback_(payloadWasRewritten ? payload.dump() : message);
	}
}

void DiscordWebSocket::sendIdentify()
{
	DiscordJson payload = {
		{ "op", 2 },
		{ "d", {
			{ "token", botToken_ },
			{ "compress", false },
			{ "large_threshold", 50 },
			{ "intents", intents_ },
			{ "properties", { { "$os", gatewayPlatform() }, { "$browser", "discord-bridge" }, { "$device", "discord-bridge" } } }
		} }
	};
	sendMessage(payload.dump());
}

void DiscordWebSocket::sendHeartbeat()
{
	DiscordJson payload = { { "op", 1 }, { "d", lastSequence_ >= 0 ? DiscordJson(lastSequence_) : DiscordJson(nullptr) } };
	sendMessage(payload.dump());
}

void DiscordWebSocket::sendResume()
{
	if (sessionId_.empty() || lastSequence_ < 0)
	{
		sendIdentify();
		return;
	}
	DiscordJson payload = {
		{ "op", 6 },
		{ "d", { { "token", botToken_ }, { "session_id", sessionId_ }, { "seq", lastSequence_ } } }
	};
	sendMessage(payload.dump());
}

std::string DiscordWebSocket::buildIdentifyPayload()
{
	DiscordJson payload = {
		{ "op", 2 },
		{ "d", { { "token", botToken_ }, { "compress", false }, { "large_threshold", 50 }, { "intents", intents_ }, { "properties", { { "$os", gatewayPlatform() }, { "$browser", "discord-bridge" }, { "$device", "discord-bridge" } } } } }
	};
	return payload.dump();
}

std::string DiscordWebSocket::buildHeartbeatPayload()
{
	DiscordJson payload = { { "op", 1 }, { "d", lastSequence_ >= 0 ? DiscordJson(lastSequence_) : DiscordJson(nullptr) } };
	return payload.dump();
}

std::string DiscordWebSocket::buildResumePayload()
{
	DiscordJson payload = {
		{ "op", 6 },
		{ "d", { { "token", botToken_ }, { "session_id", sessionId_ }, { "seq", lastSequence_ } } }
	};
	return payload.dump();
}

void DiscordWebSocket::update()
{
	// The gateway is driven by its own io_context thread.  This method remains
	// part of the component-facing implementation for compatibility.
}

void DiscordWebSocket::setGatewayUrl(const std::string& url)
{
	if (url.empty()) return;
	const bool updateInitialUrl = !identified_.load();

	std::string value = url;
	const std::string scheme = "wss://";
	if (value.compare(0, scheme.size(), scheme) == 0) value.erase(0, scheme.size());
	else return;

	const size_t pathStart = value.find('/');
	const std::string authority = pathStart == std::string::npos ? value : value.substr(0, pathStart);
	if (authority.empty()) return;

	const size_t portStart = authority.rfind(':');
	if (portStart != std::string::npos && authority.find(']') == std::string::npos)
	{
		gatewayHost_ = authority.substr(0, portStart);
		gatewayPort_ = authority.substr(portStart + 1);
	}
	else
	{
		gatewayHost_ = authority;
		gatewayPort_ = GATEWAY_PORT;
	}
	if (gatewayHost_.empty() || gatewayPort_.empty()) return;

	gatewayPath_ = pathStart == std::string::npos ? "/?v=10&encoding=json" : value.substr(pathStart);
	if (gatewayPath_.empty()) gatewayPath_ = "/?v=10&encoding=json";
	else if (gatewayPath_.find('?') == std::string::npos) gatewayPath_ += "?v=10&encoding=json";
	sslCtx_.set_verify_callback(ssl::host_name_verification(gatewayHost_));
	if (updateInitialUrl)
	{
		initialGatewayHost_ = gatewayHost_;
		initialGatewayPort_ = gatewayPort_;
		initialGatewayPath_ = gatewayPath_;
	}
}

void DiscordWebSocket::restoreInitialGatewayUrl()
{
	gatewayHost_ = initialGatewayHost_;
	gatewayPort_ = initialGatewayPort_;
	gatewayPath_ = initialGatewayPath_;
	sslCtx_.set_verify_callback(ssl::host_name_verification(gatewayHost_));
}
