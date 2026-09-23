/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#include "discord-http.hpp"
#include "discord-log.hpp"
#include "discord-json.hpp"
#include "discord-tls.hpp"
#include "utils.hpp"
#include "version.hpp"
#include <core.hpp>
#include <openssl/ssl.h>
#include <algorithm>
#include <cctype>
#include <boost/asio/steady_timer.hpp>

namespace
{
std::string httpFailureDetail(const DiscordHTTP::Response& response, const std::string& token)
{
	std::string detail = response.body.empty() ? "network or TLS failure" : response.body;
	if (!token.empty())
	{
		size_t position = 0;
		while ((position = detail.find(token, position)) != std::string::npos)
		{
			detail.replace(position, token.size(), "<redacted>");
			position += 10;
		}
	}
	for (char& character : detail)
	{
		if (character == '\r' || character == '\n') character = ' ';
	}
	if (detail.size() > 180) detail.resize(180);
	return detail;
}

void logHttpFailure(ICore* core, const std::string& endpoint, const DiscordHTTP::Response& response, const std::string& detail)
{
	DiscordLogWarning(core, std::string("[DiscordBridge] Discord request ") + endpoint +
		" failed (HTTP " + std::to_string(response.statusCode) + "): " + detail);
}
}

// All operations run on the REST worker's io_context. The request deadline
// cancels DNS as well as the socket; TLS shutdown never delays the next request.
struct DiscordHTTP::Transport
{
	net::io_context io;
	ssl::context context { ssl::context::tlsv12_client };
	tcp::resolver resolver { io };
	net::steady_timer timer { io };
	std::unique_ptr<beast::ssl_stream<beast::tcp_stream>> stream;
	beast::flat_buffer buffer;
	DiscordRateLimits::Time lastUsed {};

	Transport()
	{
		DiscordTLS::configureCertificateVerification(context);
		context.set_verify_mode(ssl::verify_peer);
		context.set_verify_callback(ssl::host_name_verification(API_HOST));
	}
	void close()
	{
		if (stream)
		{
			beast::error_code ignored;
			beast::get_lowest_layer(*stream).socket().close(ignored);
			stream.reset();
		}
		buffer.consume(buffer.size());
	}
	http::response<http::string_body> exchange(http::request<http::string_body>& request)
	{
		if (stream && DiscordRateLimits::Clock::now() - lastUsed > std::chrono::seconds(15)) close();
		const bool connected = static_cast<bool>(stream);
		if (!stream)
		{
			stream = std::make_unique<beast::ssl_stream<beast::tcp_stream>>(io, context);
			if (!SSL_set_tlsext_host_name(stream->native_handle(), API_HOST))
				throw std::runtime_error("Unable to set TLS hostname");
		}
		io.restart();
		beast::error_code failure;
		bool timedOut = false;
		http::response<http::string_body> response;
		auto finish = [&](beast::error_code error)
		{
			failure = error;
			timer.cancel();
		};
		auto write = [&]()
		{
			http::async_write(*stream, request, [&](beast::error_code error, size_t)
			{
				if (error) { finish(error); return; }
				http::async_read(*stream, buffer, response, [&](beast::error_code error, size_t) { finish(error); });
			});
		};
		timer.expires_after(std::chrono::seconds(20));
		timer.async_wait([&](beast::error_code error)
		{
			if (error) return;
			timedOut = true;
			resolver.cancel();
			beast::error_code ignored;
			beast::get_lowest_layer(*stream).socket().close(ignored);
		});
		if (connected) write();
		else resolver.async_resolve(API_HOST, API_PORT, [&](beast::error_code error, tcp::resolver::results_type results)
		{
			if (error || timedOut) { finish(error); return; }
			beast::get_lowest_layer(*stream).async_connect(results, [&](beast::error_code error, const tcp::endpoint&)
			{
				if (error) { finish(error); return; }
				stream->async_handshake(ssl::stream_base::client, [&](beast::error_code error)
				{
					if (error) { finish(error); return; }
					write();
				});
			});
		});
		io.run();
		if (timedOut || failure)
		{
			close();
			if (timedOut) throw std::runtime_error("Discord HTTP request timed out after 20 seconds");
			throw beast::system_error(failure);
		}
		lastUsed = DiscordRateLimits::Clock::now();
		if (!response.keep_alive()) close();
		return response;
	}
};

DiscordHTTP::DiscordHTTP(ICore* core, const std::string& token)
	: botToken_(token), core_(core)
{
}

DiscordHTTP::~DiscordHTTP() = default;

void DiscordHTTP::runTask(const std::function<void(DiscordHTTP&)>& task, unsigned& retries,
	DiscordRateLimits::Time eligibleAt)
{
	taskRetries_ = &retries;
	eligibleAt_ = eligibleAt;
	try { task(*this); }
	catch (...) { taskRetries_ = nullptr; throw; }
	taskRetries_ = nullptr;
}

DiscordHTTP::Response DiscordHTTP::request(http::verb method, const std::string& endpoint, const std::string& body, const std::string& auditReason)
{
	return makeRequest(method, endpoint, body, auditReason);
}

DiscordHTTP::Response DiscordHTTP::makeRequest(http::verb method, const std::string& endpoint, const std::string& body, const std::string& auditReason)
{
	std::lock_guard<std::mutex> lock(requestMutex_);
	const auto route = DiscordRateLimits::route(std::string(http::to_string(method)), endpoint);
	const auto blocked = rateLimits_.blockedUntil(route);
	const auto now = DiscordRateLimits::Clock::now();
	if (blocked > (taskRetries_ ? eligibleAt_ : now))
	{
		if (taskRetries_) throw DiscordRestDeferred(blocked);
		return Response { 429, "Discord rate limit pending", false, {}, std::chrono::duration<double>(blocked - now).count(), false };
	}
	Response response { 0, {}, false, {}, 0.0, false };
	const std::string target = std::string(API_BASE) + (endpoint.empty() || endpoint.front() == '/' ? endpoint : "/" + endpoint);
	try
	{
		if (!transport_) transport_ = std::make_unique<Transport>();
		http::request<http::string_body> req { method, target, 11 };
		req.set(http::field::host, API_HOST);
		req.set(http::field::user_agent, std::string("discord-bridge/") + DISCORD_BRIDGE_VERSION + " (open.mp)");
		req.set(http::field::authorization, "Bot " + botToken_);
		req.keep_alive(true);
		if (!auditReason.empty()) req.set("X-Audit-Log-Reason", DiscordUtils::urlEncode(auditReason));
		if (!body.empty())
		{
			req.set(http::field::content_type, "application/json");
			req.body() = body;
		}
		req.prepare_payload();
		const auto res = transport_->exchange(req);
		response.statusCode = static_cast<int>(res.result_int());
		response.body = res.body();
		response.success = response.statusCode >= 200 && response.statusCode < 300;
		for (const auto& field : res.base())
		{
			std::string key(field.name_string());
			std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			response.headers[key] = std::string(field.value());
		}
	}
	catch (const std::exception& error)
	{
		// A failed read may follow a successful send. Never automatically replay
		// an ambiguous network failure, which could duplicate a message.
		transport_.reset();
		response.body = error.what();
	}
	const auto header = [&](const char* name) -> std::string
	{
		const auto it = response.headers.find(name);
		return it == response.headers.end() ? std::string() : it->second;
	};
	if (response.statusCode == 429)
	{
		double retry = DiscordRateLimits::seconds(header("retry-after"));
		const auto error = DiscordJson::parse(response.body, nullptr, false);
		if (error.is_object())
		{
			const auto value = error.find("retry_after");
			if (value != error.end() && value->is_number()) retry = std::max(retry, DiscordRateLimits::seconds(value->dump()));
			const auto global = error.find("global");
			response.globalRateLimit = global != error.end() && global->is_boolean() && global->get<bool>();
		}
		response.retryAfter = retry < 0 ? 1.0 : retry;
		response.globalRateLimit = response.globalRateLimit || header("x-ratelimit-global") == "true" || header("x-ratelimit-scope") == "global";
	}
	rateLimits_.observe(route, response.headers, response.statusCode == 429, response.retryAfter,
		response.globalRateLimit, DiscordRateLimits::Clock::now());
	if (response.statusCode == 429 && taskRetries_ && ++*taskRetries_ < 3)
		throw DiscordRestDeferred(rateLimits_.blockedUntil(route));
	if (!response.success && endpoint != "/users/@me" && endpoint != "/gateway/bot")
	{
		const std::string detail = httpFailureDetail(response, botToken_);
		const std::string failureKey = std::to_string(response.statusCode) + ":" + detail;
		const auto now = DiscordRateLimits::Clock::now();
		if (failureKey != lastFailureLogKey_ || now - lastFailureLogAt_ >= std::chrono::minutes(1))
		{
			// Route keys omit interaction/webhook tokens.
			logHttpFailure(core_, route.key, response, detail);
			lastFailureLogKey_ = failureKey;
			lastFailureLogAt_ = now;
		}
	}
	return response;
}

DiscordHTTP::Response DiscordHTTP::getBotUser()
{
	return makeRequest(http::verb::get, "/users/@me", "", "");
}

DiscordHTTP::Response DiscordHTTP::getGatewayBot()
{
	return makeRequest(http::verb::get, "/gateway/bot", "", "");
}

DiscordHTTP::Response DiscordHTTP::getCurrentUserGuilds(const std::string& after)
{
	std::string endpoint = "/users/@me/guilds?limit=200";
	if (!after.empty()) endpoint += "&after=" + DiscordUtils::urlEncode(after);
	return makeRequest(http::verb::get, endpoint, "", "");
}

DiscordHTTP::Response DiscordHTTP::getChannel(const std::string& channelId)
{
	return makeRequest(http::verb::get, "/channels/" + channelId, "", "");
}

DiscordHTTP::Response DiscordHTTP::getMessage(const std::string& channelId, const std::string& messageId)
{
	return makeRequest(http::verb::get, "/channels/" + channelId + "/messages/" + messageId, "", "");
}

DiscordHTTP::Response DiscordHTTP::sendMessage(const std::string& channelId, const std::string& content)
{
	return makeRequest(http::verb::post, "/channels/" + channelId + "/messages", DiscordUtils::buildJsonObject(content), "");
}

DiscordHTTP::Response DiscordHTTP::sendMessagePayload(const std::string& channelId, const std::string& jsonBody)
{
	return makeRequest(http::verb::post, "/channels/" + channelId + "/messages", jsonBody, "");
}

DiscordHTTP::Response DiscordHTTP::deleteMessage(const std::string& channelId, const std::string& messageId)
{
	return makeRequest(http::verb::delete_, "/channels/" + channelId + "/messages/" + messageId, "", "");
}

DiscordHTTP::Response DiscordHTTP::editMessage(const std::string& channelId, const std::string& messageId, const std::string& newContent)
{
	return makeRequest(http::verb::patch, "/channels/" + channelId + "/messages/" + messageId, DiscordUtils::buildJsonObject(newContent), "");
}

DiscordHTTP::Response DiscordHTTP::editMessagePayload(const std::string& channelId, const std::string& messageId, const std::string& jsonBody)
{
	return makeRequest(http::verb::patch, "/channels/" + channelId + "/messages/" + messageId, jsonBody, "");
}

DiscordHTTP::Response DiscordHTTP::modifyChannel(const std::string& channelId, const std::string& jsonBody)
{
	return makeRequest(http::verb::patch, "/channels/" + channelId, jsonBody, "");
}

DiscordHTTP::Response DiscordHTTP::deleteChannel(const std::string& channelId)
{
	return makeRequest(http::verb::delete_, "/channels/" + channelId, "", "");
}

DiscordHTTP::Response DiscordHTTP::addReaction(const std::string& channelId, const std::string& messageId, const std::string& emoji)
{
	return makeRequest(http::verb::put, "/channels/" + channelId + "/messages/" + messageId + "/reactions/" + DiscordUtils::urlEncode(emoji) + "/@me", "", "");
}

DiscordHTTP::Response DiscordHTTP::deleteOwnReaction(const std::string& channelId, const std::string& messageId, const std::string& emoji)
{
	return makeRequest(http::verb::delete_, "/channels/" + channelId + "/messages/" + messageId + "/reactions/" + DiscordUtils::urlEncode(emoji) + "/@me", "", "");
}

DiscordHTTP::Response DiscordHTTP::deleteAllReactions(const std::string& channelId, const std::string& messageId)
{
	return makeRequest(http::verb::delete_, "/channels/" + channelId + "/messages/" + messageId + "/reactions", "", "");
}

DiscordHTTP::Response DiscordHTTP::deleteEmojiReactions(const std::string& channelId, const std::string& messageId, const std::string& emoji)
{
	return makeRequest(http::verb::delete_, "/channels/" + channelId + "/messages/" + messageId + "/reactions/" + DiscordUtils::urlEncode(emoji), "", "");
}

DiscordHTTP::Response DiscordHTTP::getGuild(const std::string& guildId)
{
	return makeRequest(http::verb::get, "/guilds/" + guildId, "", "");
}

DiscordHTTP::Response DiscordHTTP::getGuildRoles(const std::string& guildId)
{
	return makeRequest(http::verb::get, "/guilds/" + guildId + "/roles", "", "");
}

DiscordHTTP::Response DiscordHTTP::modifyGuild(const std::string& guildId, const std::string& jsonBody)
{
	return makeRequest(http::verb::patch, "/guilds/" + guildId, jsonBody, "");
}

DiscordHTTP::Response DiscordHTTP::getGuildChannels(const std::string& guildId)
{
	return makeRequest(http::verb::get, "/guilds/" + guildId + "/channels", "", "");
}

DiscordHTTP::Response DiscordHTTP::createGuildChannel(const std::string& guildId, const std::string& jsonBody)
{
	return makeRequest(http::verb::post, "/guilds/" + guildId + "/channels", jsonBody, "");
}

DiscordHTTP::Response DiscordHTTP::createDM(const std::string& recipientId)
{
	return makeRequest(http::verb::post, "/users/@me/channels", std::string("{") + DiscordUtils::buildJsonPair("recipient_id", recipientId) + "}", "");
}

DiscordHTTP::Response DiscordHTTP::triggerTyping(const std::string& channelId)
{
	return makeRequest(http::verb::post, "/channels/" + channelId + "/typing", "", "");
}

DiscordHTTP::Response DiscordHTTP::getUser(const std::string& userId)
{
	return makeRequest(http::verb::get, "/users/" + userId, "", "");
}

DiscordHTTP::Response DiscordHTTP::getGuildMember(const std::string& guildId, const std::string& userId)
{
	return makeRequest(http::verb::get, "/guilds/" + guildId + "/members/" + userId, "", "");
}

DiscordHTTP::Response DiscordHTTP::modifyGuildMember(const std::string& guildId, const std::string& userId, const std::string& jsonBody, const std::string& reason)
{
	return makeRequest(http::verb::patch, "/guilds/" + guildId + "/members/" + userId, jsonBody, reason);
}

DiscordHTTP::Response DiscordHTTP::addGuildMemberRole(const std::string& guildId, const std::string& userId, const std::string& roleId, const std::string& reason)
{
	return makeRequest(http::verb::put, "/guilds/" + guildId + "/members/" + userId + "/roles/" + roleId, "", reason);
}

DiscordHTTP::Response DiscordHTTP::removeGuildMemberRole(const std::string& guildId, const std::string& userId, const std::string& roleId, const std::string& reason)
{
	return makeRequest(http::verb::delete_, "/guilds/" + guildId + "/members/" + userId + "/roles/" + roleId, "", reason);
}

DiscordHTTP::Response DiscordHTTP::removeGuildMember(const std::string& guildId, const std::string& userId, const std::string& reason)
{
	return makeRequest(http::verb::delete_, "/guilds/" + guildId + "/members/" + userId, "", reason);
}

DiscordHTTP::Response DiscordHTTP::createGuildMemberBan(const std::string& guildId, const std::string& userId, const std::string& reason, int deleteMessageSeconds)
{
	const int seconds = std::max(0, std::min(deleteMessageSeconds, 604800));
	const std::string body = "{\"delete_message_seconds\":" + std::to_string(seconds) + "}";
	return makeRequest(http::verb::put, "/guilds/" + guildId + "/bans/" + userId, body, reason);
}

DiscordHTTP::Response DiscordHTTP::removeGuildMemberBan(const std::string& guildId, const std::string& userId, const std::string& reason)
{
	return makeRequest(http::verb::delete_, "/guilds/" + guildId + "/bans/" + userId, "", reason);
}

DiscordHTTP::Response DiscordHTTP::modifyGuildRole(const std::string& guildId, const std::string& roleId, const std::string& jsonBody)
{
	return makeRequest(http::verb::patch, "/guilds/" + guildId + "/roles/" + roleId, jsonBody, "");
}

DiscordHTTP::Response DiscordHTTP::deleteGuildRole(const std::string& guildId, const std::string& roleId)
{
	return makeRequest(http::verb::delete_, "/guilds/" + guildId + "/roles/" + roleId, "", "");
}

DiscordHTTP::Response DiscordHTTP::createGuildRole(const std::string& guildId, const std::string& jsonBody)
{
	return makeRequest(http::verb::post, "/guilds/" + guildId + "/roles", jsonBody, "");
}

DiscordHTTP::Response DiscordHTTP::modifyGuildRolePosition(const std::string& guildId, const std::string& roleId, int position)
{
	const std::string body = std::string("[{\"id\":\"") + DiscordUtils::escapeJson(roleId) + "\",\"position\":" + std::to_string(position) + "}]";
	return makeRequest(http::verb::patch, "/guilds/" + guildId + "/roles", body, "");
}

DiscordHTTP::Response DiscordHTTP::getApplicationCommands(const std::string& guildId)
{
	if (applicationId_.empty()) return Response { 0, {}, false, {}, 0.0, false };
	const std::string endpoint = guildId.empty()
		? "/applications/" + applicationId_ + "/commands"
		: "/applications/" + applicationId_ + "/guilds/" + guildId + "/commands";
	return makeRequest(http::verb::get, endpoint, "", "");
}

DiscordHTTP::Response DiscordHTTP::createApplicationCommand(const std::string& guildId, const std::string& jsonBody)
{
	if (applicationId_.empty())
	{
		return Response { 0, "application id is not available", false, {}, 0.0, false };
	}
	const std::string scope = guildId.empty() ? "/applications/" + applicationId_ + "/commands" : "/applications/" + applicationId_ + "/guilds/" + guildId + "/commands";
	return makeRequest(http::verb::post, scope, jsonBody, "");
}

DiscordHTTP::Response DiscordHTTP::deleteApplicationCommand(const std::string& guildId, const std::string& commandId)
{
	if (applicationId_.empty())
	{
		return Response { 0, "application id is not available", false, {}, 0.0, false };
	}
	const std::string scope = guildId.empty() ? "/applications/" + applicationId_ + "/commands/" + commandId : "/applications/" + applicationId_ + "/guilds/" + guildId + "/commands/" + commandId;
	return makeRequest(http::verb::delete_, scope, "", "");
}

DiscordHTTP::Response DiscordHTTP::createInteractionResponse(const std::string& interactionId, const std::string& interactionToken, const std::string& jsonBody)
{
	return makeRequest(http::verb::post, "/interactions/" + interactionId + "/" + interactionToken + "/callback", jsonBody, "");
}

DiscordHTTP::Response DiscordHTTP::editOriginalInteractionResponse(const std::string& interactionToken, const std::string& jsonBody)
{
	if (applicationId_.empty())
	{
		return Response { 0, "application id is not available", false, {}, 0.0, false };
	}
	return makeRequest(http::verb::patch, "/webhooks/" + applicationId_ + "/" + interactionToken + "/messages/@original", jsonBody, "");
}
