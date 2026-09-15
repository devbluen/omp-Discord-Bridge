#include "discord-mentions.hpp"
#include <iostream>

int main()
{
	const auto options = DiscordJson::parse(R"([
		{"type":3,"value":"<@123> <@!456> <@123> <@&789> <#789> <@! 987 >"},
		{"type":6,"value":"123"},
		{"type":2,"options":[{"type":1,"options":[{"type":3,"value":"nested <@654>"}]}]},
		{"type":3,"value":"<@> <@!> <@0> <@-1> <@123x> <@18446744073709551616> <@18446744073709551615> <@123"},
		{"type":3,"value":42}, null, {"type":"3","value":"<@111>"}
	])");
	std::vector<std::string> users;
	DiscordMentions::collect(options, users);
	if (users != std::vector<std::string>({"123", "456", "123", "987", "654", "18446744073709551615"}))
	{
		std::cerr << "FAIL: mention extraction order, duplicates, nesting or validation\n";
		return 1;
	}
	users.clear();
	DiscordMentions::collect(nullptr, users);
	DiscordMentions::collect(DiscordJson::parse(R"([{"type":3,"value":"no mentions"},{"type":1,"options":{}}])"), users);
	if (!users.empty()) return 1;
	std::cout << "Interaction mention tests passed\n";
}
