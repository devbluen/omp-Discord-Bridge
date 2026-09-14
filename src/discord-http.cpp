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
#include <thread>

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

DiscordHTTP::DiscordHTTP(ICore* core, const std::string& token)
	: botToken_(token)
	, core_(core)
{
}

DiscordHTTP::Response DiscordHTTP::request(http::verb method, const std::string& endpoint, const std::string& body, const std::string& auditReason)
{
	return makeRequest(method, endpoint, body, auditReason);
}

DiscordHTTP::Response DiscordHTTP::makeRequest(http::verb method, const std::string& endpoint, const std::string& body, const std::string& auditReason)
{
	std::lock_guard<std::mutex> lock(requestMutex_);
	Response response { 0, {}, false, {}, 0.0, false };

	const std::string target = std::string(API_BASE) + (endpoint.empty() || endpoint.front() == '/' ? endpoint : "/" + endpoint);
	for (int attempt = 0; attempt < 3; ++attempt)
	{
		response = Response { 0, {}, false, {}, 0.0, false };
		const auto now = std::chrono::steady_clock::now();
		if (now < globalBlockedUntil_)
		{
			std::this_thread::sleep_for(globalBlockedUntil_ - now);
		}

		try
		{
			net::io_context ioc;
			ssl::context ctx(ssl::context::tlsv12_client);
			DiscordTLS::configureCertificateVerification(ctx);
			ctx.set_verify_mode(ssl::verify_peer);
			ctx.set_verify_callback(ssl::host_name_verification(API_HOST));

			tcp::resolver resolver(ioc);
			beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);
			if (!SSL_set_tlsext_host_name(stream.native_handle(), API_HOST))
			{
				throw beast::system_error(beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()));
			}

			auto const results = resolver.resolve(API_HOST, API_PORT);
			beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(20));
			beast::get_lowest_layer(stream).connect(results);
			stream.handshake(ssl::stream_base::client);

			http::request<http::string_body> req { method, target, 11 };
			req.set(http::field::host, API_HOST);
			req.set(http::field::user_agent, std::string("discord-bridge/") + DISCORD_BRIDGE_VERSION + " (open.mp)");
			req.set(http::field::authorization, "Bot " + botToken_);
			if (!auditReason.empty()) req.set("X-Audit-Log-Reason", DiscordUtils::urlEncode(auditReason));
			if (!body.empty())
			{
				req.set(http::field::content_type, "application/json");
				req.body() = body;
			}
			req.prepare_payload();

			http::write(stream, req);

			beast::flat_buffer buffer;
			http::response<http::string_body> res;
			http::read(stream, buffer, res);

			response.statusCode = static_cast<int>(res.result_int());
			response.body = res.body();
			response.success = response.statusCode >= 200 && response.statusCode < 300;
			for (const auto& field : res.base())
			{
				std::string key(field.name_string());
				std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				response.headers[key] = std::string(field.value().data(), field.value().size());
			}

			const auto retryHeader = response.headers.find("retry-after");
			if (retryHeader != response.headers.end())
			{
				try { response.retryAfter = std::stod(retryHeader->second); } catch (...) { response.retryAfter = 0.0; }
			}
			if (response.statusCode == 429)
			{
				const DiscordJson error = DiscordJson::parse(response.body, nullptr, false);
				if (!error.is_discarded() && error.is_object() && (error.find("retry_after") != error.end()) && error["retry_after"].is_number())
				{
					response.retryAfter = std::max(response.retryAfter, error["retry_after"].get<double>());
				}
				const auto globalHeader = response.headers.find("x-ratelimit-global");
				response.globalRateLimit = globalHeader != response.headers.end() && globalHeader->second == "true";
				if (response.globalRateLimit)
				{
					globalBlockedUntil_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<long long>(response.retryAfter * 1000.0) + 250);
				}
			}

			beast::error_code ec;
			stream.shutdown(ec);
			if (ec == net::error::eof || ec == ssl::error::stream_truncated)
			{
				ec = {};
			}
		}
		catch (const std::exception& e)
		{
			response.body = e.what();
			response.success = false;
		}

		if (response.statusCode != 429 || response.retryAfter <= 0.0 || attempt == 2)
		{
			// Startup has a dedicated, human-readable warning in DiscordBot;
			// other REST failures are reported here once after their retries.
			if (!response.success && endpoint != "/users/@me" && endpoint != "/gateway/bot")
			{
				const std::string detail = httpFailureDetail(response, botToken_);
				const std::string failureKey = std::to_string(response.statusCode) + ":" + detail;
				const auto now = std::chrono::steady_clock::now();
				if (failureKey != lastFailureLogKey_ || now - lastFailureLogAt_ >= std::chrono::minutes(1))
				{
					logHttpFailure(core_, target, response, detail);
					lastFailureLogKey_ = failureKey;
					lastFailureLogAt_ = now;
				}
			}
			return response;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(response.retryAfter * 1000.0) + 250));
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
