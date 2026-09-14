#include "utils.hpp"

#include <iostream>
#include <string>

namespace
{
bool expect(bool condition, const char* message)
{
	if (!condition) std::cerr << "FAIL: " << message << '\n';
	return condition;
}
}

int main()
{
	bool ok = true;
	ok &= expect(DiscordUtils::escapeJson("quote\" slash\\ line\n") == "quote\\\" slash\\\\ line\\n", "JSON escaping");
	ok &= expect(DiscordUtils::unescapeJson("quote\\\" slash\\\\ line\\n") == "quote\" slash\\ line\n", "JSON unescaping");
	ok &= expect(DiscordUtils::urlEncode("a b/c?x=1") == "a%20b%2Fc%3Fx%3D1", "URL encoding");
	ok &= expect(DiscordUtils::base64Encode("M") == "TQ==" && DiscordUtils::base64Encode("Ma") == "TWE=" &&
		DiscordUtils::base64Encode("Man") == "TWFu", "base64 encoding");
	ok &= expect(DiscordUtils::buildJsonObject("hello\"world") == "{\"content\":\"hello\\\"world\"}", "JSON object construction");
	ok &= expect(DiscordUtils::extractJsonString("{\"name\":\"a\\\"b\"}", "name") == "a\"b", "JSON string extraction");
	ok &= expect(DiscordUtils::extractJsonInt("{\"position\": -7}", "position") == -7, "JSON integer extraction");
	ok &= expect(DiscordUtils::extractJsonBool("{\"enabled\": true}", "enabled"), "JSON boolean extraction");
	ok &= expect(DiscordUtils::stringToSnowflake("123456789") == 123456789ULL, "snowflake parsing");
	ok &= expect(DiscordUtils::stringToSnowflake("not-a-snowflake") == 0, "invalid snowflake rejection");
	ok &= expect(DiscordUtils::parseDiscordTimestamp("1970-01-01T00:00:01.000Z") == 1, "Discord timestamp parsing");

	return ok ? 0 : 1;
}
