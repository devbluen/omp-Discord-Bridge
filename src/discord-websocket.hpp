/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <string>
#include <functional>
#include <atomic>
#include <queue>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <cstdint>
#include <memory>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

class DiscordBot;
struct ICore;

class DiscordWebSocket
{
public:
	using MessageCallback = std::function<void(const std::string&)>;

private:
	using WebSocketStream = websocket::stream<ssl::stream<beast::tcp_stream>>;

	net::io_context ioc_;
	ssl::context sslCtx_;
	tcp::resolver resolver_;
	std::shared_ptr<WebSocketStream> ws_;

	std::string botToken_;
	int intents_;
	int heartbeatInterval_;
	int64_t lastSequence_;
	std::string sessionId_;
	std::string resumeGatewayUrl_;
	std::string gatewayHost_;
	std::string gatewayPort_;
	std::string gatewayPath_;
	std::string initialGatewayHost_;
	std::string initialGatewayPort_;
	std::string initialGatewayPath_;

	std::atomic<bool> connected_;
	std::atomic<bool> identified_;
	std::atomic<bool> shouldStop_;

	MessageCallback messageCallback_;
	DiscordBot* bot_;
	ICore* core_;

	std::deque<std::shared_ptr<std::string>> sendQueue_;
	std::size_t sendQueueBytes_ = 0;
	std::atomic<std::size_t> pendingSendCount_ { 0 };
	std::atomic<std::size_t> pendingSendBytes_ { 0 };
	bool writeInProgress_ = false;
	bool sendQueueLimitLogged_ = false;
	net::steady_timer heartbeatTimer_;
	net::steady_timer reconnectTimer_;
	std::thread networkThread_;
	std::mutex networkStateMutex_;
	std::condition_variable networkStateCondition_;
	bool networkStarted_ = false;
	bool networkRunning_ = false;
	unsigned int reconnectAttempt_ = 0;
	bool reconnectScheduled_ = false;
	std::chrono::steady_clock::time_point lastHeartbeat_;
	std::string lastGatewayErrorKey_;
	std::chrono::steady_clock::time_point lastGatewayErrorAt_{};

	static constexpr std::size_t MAX_SEND_QUEUE_MESSAGES = 1024;
	static constexpr std::size_t MAX_SEND_QUEUE_BYTES = 1024 * 1024;
	static constexpr std::size_t MAX_GATEWAY_MESSAGE_BYTES = 16 * 1024 * 1024;

	static constexpr const char* GATEWAY_HOST = "gateway.discord.gg";
	static constexpr const char* GATEWAY_PORT = "443";
	static constexpr const char* GATEWAY_PATH = "/?v=10&encoding=json";

	void logGatewayError(const char* stage, const beast::error_code& ec);
	// Logs an explanation and returns true for close codes that make reconnecting pointless.
	bool reportFatalClose(int code);
	void handleMessage(const std::string& message);
	void run();
	void startResolve();
	void onResolve(beast::error_code ec, tcp::resolver::results_type results);
	void onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type endpoint, std::shared_ptr<WebSocketStream> connection);
	void onSslHandshake(beast::error_code ec, std::shared_ptr<WebSocketStream> connection);
	void onHandshake(beast::error_code ec, std::shared_ptr<WebSocketStream> connection);
	void readNext(std::shared_ptr<WebSocketStream> connection, std::shared_ptr<beast::flat_buffer> readBuffer);
	void onRead(beast::error_code ec, std::size_t bytesTransferred, std::shared_ptr<WebSocketStream> connection, std::shared_ptr<beast::flat_buffer> readBuffer);
	void scheduleReconnect();
	void onReconnect(beast::error_code ec);
	void scheduleHeartbeat();
	void onHeartbeat(beast::error_code ec);
	void queueWrite(std::string message);
	void writeNext(std::shared_ptr<WebSocketStream> connection);
	void onWrite(beast::error_code ec, std::size_t bytesTransferred, std::shared_ptr<WebSocketStream> connection, std::shared_ptr<std::string> message);
	void abandonConnection(std::shared_ptr<WebSocketStream> connection);
	void shutdownOnIoThread();
	void sendIdentify();
	void sendHeartbeat();
	void sendResume();
	void restoreInitialGatewayUrl();
	std::string buildIdentifyPayload();
	std::string buildHeartbeatPayload();
	std::string buildResumePayload();

public:
	DiscordWebSocket(DiscordBot* bot, ICore* core, const std::string& token, int intents);
	~DiscordWebSocket();

	bool connect();
	void disconnect();
	bool isConnected() const { return connected_.load(); }

	void setMessageCallback(MessageCallback callback) { messageCallback_ = callback; }

	bool sendMessage(const std::string& message);
	bool sendPresenceUpdate(int status, int activityType, const std::string& activityName, const std::string& activityUrl);

	void requestGuildMembers(const std::string& guildId);
	void update();
	void setGatewayUrl(const std::string& url);
};
