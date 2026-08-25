/*
 *  Discord Bridge for open.mp
 *  Copyright (c) 2026 Neufox
 */

#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <chrono>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

struct ICore;

class DiscordHTTP
{
public:
	struct Response
	{
		int statusCode;
		std::string body;
		bool success;
		std::unordered_map<std::string, std::string> headers;
		double retryAfter = 0.0;
		bool globalRateLimit = false;
	};

private:
	std::string botToken_;
	std::string applicationId_;
	ICore* core_;
	mutable std::mutex requestMutex_;
	std::chrono::steady_clock::time_point globalBlockedUntil_{};
	std::string lastFailureLogKey_;
	std::chrono::steady_clock::time_point lastFailureLogAt_{};

	static constexpr const char* API_HOST = "discord.com";
	static constexpr const char* API_BASE = "/api/v10";
	static constexpr const char* API_PORT = "443";

	Response makeRequest(http::verb method, const std::string& endpoint, const std::string& body = "");

public:
	DiscordHTTP(ICore* core, const std::string& token);
	~DiscordHTTP() = default;

	Response getBotUser();
	Response getGatewayBot();
	Response getCurrentUserGuilds(const std::string& after = {});
	void setApplicationId(const std::string& applicationId) { applicationId_ = applicationId; }
	const std::string& getApplicationId() const { return applicationId_; }

	Response getChannel(const std::string& channelId);
	Response getMessage(const std::string& channelId, const std::string& messageId);
	Response sendMessage(const std::string& channelId, const std::string& content);
	Response sendMessagePayload(const std::string& channelId, const std::string& jsonBody);
	Response deleteMessage(const std::string& channelId, const std::string& messageId);
	Response editMessage(const std::string& channelId, const std::string& messageId, const std::string& newContent);
	Response editMessagePayload(const std::string& channelId, const std::string& messageId, const std::string& jsonBody);
	Response modifyChannel(const std::string& channelId, const std::string& jsonBody);
	Response deleteChannel(const std::string& channelId);
	Response addReaction(const std::string& channelId, const std::string& messageId, const std::string& emoji);
	Response deleteOwnReaction(const std::string& channelId, const std::string& messageId, const std::string& emoji);
	Response deleteAllReactions(const std::string& channelId, const std::string& messageId);
	Response deleteEmojiReactions(const std::string& channelId, const std::string& messageId, const std::string& emoji);

	Response getGuild(const std::string& guildId);
	Response getGuildRoles(const std::string& guildId);
	Response modifyGuild(const std::string& guildId, const std::string& jsonBody);
	Response getGuildChannels(const std::string& guildId);
	Response createGuildChannel(const std::string& guildId, const std::string& jsonBody);
	Response createDM(const std::string& recipientId);
	Response triggerTyping(const std::string& channelId);

	Response getUser(const std::string& userId);

	Response getGuildMember(const std::string& guildId, const std::string& userId);
	Response modifyGuildMember(const std::string& guildId, const std::string& userId, const std::string& jsonBody);
	Response addGuildMemberRole(const std::string& guildId, const std::string& userId, const std::string& roleId);
	Response removeGuildMemberRole(const std::string& guildId, const std::string& userId, const std::string& roleId);
	Response removeGuildMember(const std::string& guildId, const std::string& userId);
	Response createGuildMemberBan(const std::string& guildId, const std::string& userId, const std::string& reason = "");
	Response removeGuildMemberBan(const std::string& guildId, const std::string& userId);

	Response modifyGuildRole(const std::string& guildId, const std::string& roleId, const std::string& jsonBody);
	Response deleteGuildRole(const std::string& guildId, const std::string& roleId);
	Response createGuildRole(const std::string& guildId, const std::string& jsonBody);
	Response modifyGuildRolePosition(const std::string& guildId, const std::string& roleId, int position);

	Response getApplicationCommands(const std::string& guildId = {});
	Response createApplicationCommand(const std::string& guildId, const std::string& jsonBody);
	Response deleteApplicationCommand(const std::string& guildId, const std::string& commandId);
	Response createInteractionResponse(const std::string& interactionId, const std::string& interactionToken, const std::string& jsonBody);
	Response editOriginalInteractionResponse(const std::string& interactionToken, const std::string& jsonBody);
};
