#include "discord-builders.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace DiscordBuilders;

namespace
{
bool expect(bool condition, const char* message)
{
	if (!condition) std::cerr << "FAIL: " << message << '\n';
	return condition;
}

bool embedTests()
{
	bool ok = true;
	Embed embed;
	embed.title = "Title";
	embed.color = 0x1FFAABB;
	embed.authorName = "Author";
	embed.authorIconUrl = "https://example.com/a.png";
	embed.fields.push_back({ "Name", "Value", true });
	const DiscordJson json = embed.toJson();
	ok &= expect(json["color"] == 0xFFAABB, "embed colour is masked to 24 bits");
	ok &= expect(json["author"]["name"] == "Author" && json["author"]["icon_url"] == "https://example.com/a.png", "embed author");
	ok &= expect(json["author"].find("url") == json["author"].end(), "empty author URL is omitted");
	ok &= expect(json["fields"][0]["inline"] == true, "inline embed field");
	ok &= expect(embed.characterCount() == 5 + 6 + 4 + 5, "embed character count");
	return ok;
}

bool emojiTests()
{
	bool ok = true;
	ok &= expect(parseEmoji("🔥") == DiscordJson({ { "name", "🔥" } }), "unicode emoji");
	ok &= expect(parseEmoji("<:ok:123>") == DiscordJson({ { "id", "123" }, { "name", "ok" } }), "custom emoji mention");
	ok &= expect(parseEmoji("<a:spin:456>")["animated"] == true, "animated custom emoji");
	ok &= expect(parseEmoji("wave:789")["id"] == "789", "short custom emoji form");
	ok &= expect(parseEmoji("").is_null(), "empty emoji");
	return ok;
}

Handle makeButton(ComponentStore& store, const std::string& id)
{
	const Handle button = store.create(Button);
	store.get(button)->data["custom_id"] = id;
	store.get(button)->data["label"] = id;
	return button;
}

bool componentTests()
{
	bool ok = true;
	std::string error;
	ComponentStore store;

	const Handle row = store.create(ActionRow);
	for (int i = 0; i < 5; ++i) ok &= expect(store.addChild(row, makeButton(store, "b" + std::to_string(i)), error), "five buttons fit in a row");
	ok &= expect(!store.addChild(row, makeButton(store, "b5"), error), "a sixth button is rejected");

	const Handle mixed = store.create(ActionRow);
	ok &= expect(store.addChild(mixed, makeButton(store, "x"), error), "button in fresh row");
	const Handle menu = store.create(StringSelect);
	ok &= expect(!store.addChild(mixed, menu, error), "select menu cannot join a button row");

	const Handle attached = makeButton(store, "once");
	const Handle otherRow = store.create(ActionRow);
	const Handle thirdRow = store.create(ActionRow);
	ok &= expect(store.addChild(otherRow, attached, error), "attach button");
	ok &= expect(!store.addChild(thirdRow, attached, error), "a component has a single parent");
	ok &= expect(!store.addChild(otherRow, otherRow, error), "a component cannot contain itself");

	const Handle section = store.create(Section);
	const Handle text = store.create(TextDisplay);
	store.get(text)->data["content"] = "hello";
	ok &= expect(store.addChild(section, text, error), "text display in section");
	ok &= expect(!store.validate(section, error), "section without accessory is invalid");
	const Handle thumbnail = store.create(Thumbnail);
	store.get(thumbnail)->data["media"] = { { "url", "https://example.com/t.png" } };
	ok &= expect(store.setAccessory(section, thumbnail, error), "thumbnail accessory");
	ok &= expect(store.validate(section, error), "complete section validates");

	const Handle container = store.create(Container);
	store.get(container)->data["accent_color"] = 0x5865F2;
	ok &= expect(store.addChild(container, section, error), "section in container");
	const DiscordJson rendered = store.render(container);
	ok &= expect(rendered["type"] == Container && rendered["components"][0]["type"] == Section, "container renders its children");
	ok &= expect(rendered["components"][0]["accessory"]["type"] == Thumbnail, "section renders its accessory");

	const size_t before = store.size();
	ok &= expect(store.destroy(container), "destroy container");
	ok &= expect(store.size() == before - 4, "destroying a tree removes every owned component");

	const Handle label = store.create(Label);
	store.get(label)->data["label"] = "Name";
	const Handle input = store.create(TextInput);
	store.get(input)->data["custom_id"] = "name";
	ok &= expect(store.addChild(label, input, error), "text input in label");
	ok &= expect(store.render(label)["component"]["custom_id"] == "name", "label renders a single component");
	ok &= expect(!store.addChild(label, store.create(TextInput), error), "a label wraps one component");
	return ok;
}

bool messageTests()
{
	bool ok = true;
	std::string error;
	ComponentStore store;
	DiscordJson out;

	MessageBuilder empty;
	ok &= expect(!renderMessage(empty, store, false, out, error), "empty message is rejected");

	MessageBuilder classic;
	classic.content = "hi";
	Embed embed;
	embed.title = "t";
	classic.embeds.push_back(embed);
	const Handle row = store.create(ActionRow);
	store.addChild(row, makeButton(store, "go"), error);
	classic.components.push_back(row);
	ok &= expect(renderMessage(classic, store, true, out, error), "classic message renders");
	ok &= expect(out["content"] == "hi" && out["embeds"].size() == 1 && out["components"].size() == 1, "classic message fields");
	ok &= expect(out["flags"] == MESSAGE_FLAG_EPHEMERAL, "ephemeral flag");

	MessageBuilder v2;
	v2.content = "header";
	const Handle container = store.create(Container);
	const Handle text = store.create(TextDisplay);
	store.get(text)->data["content"] = "body";
	store.addChild(container, text, error);
	v2.components.push_back(container);
	v2.replyMessageId = "123456789012345678";
	v2.replyMention = false;
	ok &= expect(renderMessage(v2, store, false, out, error), "Components V2 message renders");
	ok &= expect((out["flags"].get<int>() & MESSAGE_FLAG_COMPONENTS_V2) != 0, "Components V2 flag is set automatically");
	ok &= expect(out.find("content") == out.end() && out["components"][0]["type"] == TextDisplay &&
		out["components"][0]["content"] == "header", "content becomes a leading text display");
	ok &= expect(out["allowed_mentions"]["replied_user"] == false, "reply without mention");

	v2.embeds.push_back(embed);
	ok &= expect(!renderMessage(v2, store, false, out, error), "Components V2 messages reject embeds");

	MessageBuilder invalid;
	invalid.components.push_back(makeButton(store, "loose"));
	ok &= expect(!renderMessage(invalid, store, false, out, error), "a loose button is not a valid top-level component");
	return ok;
}

bool modalTests()
{
	bool ok = true;
	std::string error;
	ComponentStore store;
	DiscordJson out;

	Modal modal;
	modal.customId = "report";
	modal.title = "Report a player";
	const Handle label = store.create(Label);
	store.get(label)->data["label"] = "Reason";
	const Handle input = store.create(TextInput);
	store.get(input)->data["custom_id"] = "reason";
	store.addChild(label, input, error);
	modal.components.push_back(label);
	ok &= expect(renderModal(modal, store, out, error), "modal renders");
	ok &= expect(out["components"][0]["component"]["custom_id"] == "reason", "modal label component");

	modal.components.push_back(makeButton(store, "nope"));
	ok &= expect(!renderModal(modal, store, out, error), "buttons are not allowed in modals");
	return ok;
}

bool commandTests()
{
	bool ok = true;
	std::string error;
	CommandStore store;

	ok &= expect(!store.createCommand(ChatInputCommand, "Upper", "desc", "", error), "uppercase slash command names are rejected");
	ok &= expect(!store.createCommand(ChatInputCommand, "empty", "", "", error), "slash commands need a description");

	const Handle admin = store.createCommand(ChatInputCommand, "admin", "Admin tools", "", error);
	ok &= expect(admin != 0, "create slash command");
	ok &= expect(!store.createCommand(ChatInputCommand, "admin", "Again", "", error), "duplicate command in the same scope");
	ok &= expect(store.createCommand(ChatInputCommand, "admin", "Guild copy", "123", error) != 0, "same name in another scope");

	const Handle group = store.addOption(admin, 0, SubCommandGroupOption, "player", "Player tools", false, error);
	const Handle kick = store.addOption(admin, group, SubCommandOption, "kick", "Kick a player", false, error);
	ok &= expect(group && kick, "subcommand group and subcommand");
	ok &= expect(!store.addOption(admin, 0, StringOption, "loose", "Loose option", false, error), "options cannot mix with subcommands");
	ok &= expect(!store.addOption(admin, group, StringOption, "bad", "Bad nesting", false, error), "groups only hold subcommands");

	const Handle reason = store.addOption(admin, kick, StringOption, "reason", "Reason", false, error);
	const Handle target = store.addOption(admin, kick, UserOption, "target", "Target", true, error);
	const Handle amount = store.addOption(admin, kick, IntegerOption, "amount", "Amount", false, error);
	ok &= expect(reason && target && amount, "subcommand options");
	ok &= expect(!store.addChoice(reason, "One", 1, error), "choice value type must match");
	ok &= expect(store.addChoice(reason, "Spam", "spam", error), "string choice");
	ok &= expect(!store.setAutocomplete(reason, true, error), "autocomplete cannot combine with choices");
	store.getOption(amount)->minValue = 1.0;
	store.getOption(amount)->maxValue = 10.4;

	const DiscordJson json = store.renderCommand(admin);
	const DiscordJson& kickJson = json["options"][0]["options"][0];
	ok &= expect(kickJson["name"] == "kick" && kickJson.find("required") == kickJson.end(), "subcommands never render required");
	ok &= expect(kickJson["options"][0]["name"] == "target", "required options are rendered first");
	ok &= expect(kickJson["options"][2]["max_value"].is_number_integer() && kickJson["options"][2]["max_value"] == 10, "integer ranges render as integers");

	const Handle userCommand = store.createCommand(UserCommand, "Report User", "ignored", "", error);
	ok &= expect(userCommand != 0, "context menu names may contain spaces");
	const DiscordJson userJson = store.renderCommand(userCommand);
	ok &= expect(userJson.find("description") == userJson.end(), "context menu commands have no description");
	ok &= expect(!store.addOption(userCommand, 0, StringOption, "x", "x", false, error), "context menu commands have no options");

	ok &= expect(store.renderScope("").size() == 2 && store.renderScope("123").size() == 1, "commands are grouped by scope");
	ok &= expect(store.destroyCommand(admin) && !store.getOption(kick), "destroying a command removes its options");
	return ok;
}

bool interactionTests()
{
	bool ok = true;
	const DiscordJson command = DiscordJson::parse(R"({
		"name": "admin",
		"options": [{ "type": 2, "name": "player", "options": [{ "type": 1, "name": "kick", "options": [
			{ "type": 6, "name": "target", "value": "123456789012345678" },
			{ "type": 3, "name": "reason", "value": "spa", "focused": true }
		]}]}]
	})");
	const DiscordJson* target = findCommandOption(command, "target");
	ok &= expect(target && (*target)["value"] == "123456789012345678", "nested option lookup");
	ok &= expect(!findCommandOption(command, "kick"), "subcommands are not returned as options");
	ok &= expect(findSubcommand(command, true) == "player" && findSubcommand(command, false) == "kick", "subcommand names");
	const DiscordJson* focused = findFocusedOption(command);
	ok &= expect(focused && (*focused)["name"] == "reason", "focused autocomplete option");

	std::vector<std::string> values;
	collectSubmittedValues(DiscordJson::parse(R"({ "custom_id": "menu", "values": ["a", "b"] })"), "", values);
	ok &= expect(values == std::vector<std::string>({ "a", "b" }), "select menu values");

	const DiscordJson modal = DiscordJson::parse(R"({ "custom_id": "report", "components": [
		{ "type": 1, "components": [{ "type": 4, "custom_id": "legacy", "value": "row value" }] },
		{ "type": 18, "component": { "type": 3, "custom_id": "pick", "values": ["x"] } }
	]})");
	values.clear();
	collectSubmittedValues(modal, "legacy", values);
	ok &= expect(values == std::vector<std::string>({ "row value" }), "text input inside an action row");
	values.clear();
	collectSubmittedValues(modal, "pick", values);
	ok &= expect(values == std::vector<std::string>({ "x" }), "select menu inside a label");
	values.clear();
	collectSubmittedValues(modal, "missing", values);
	ok &= expect(values.empty(), "unknown custom id");
	ok &= expect(jsonScalarToString(DiscordJson(42)) == "42" && jsonScalarToString(DiscordJson(true)) == "true", "scalar conversion");
	return ok;
}
}

int main()
{
	bool ok = true;
	try
	{
		ok &= embedTests();
		ok &= emojiTests();
		ok &= componentTests();
		ok &= messageTests();
		ok &= modalTests();
		ok &= commandTests();
		ok &= interactionTests();
	}
	catch (const std::exception& error)
	{
		std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
		ok = false;
	}
	if (ok) std::cout << "all builder tests passed\n";
	return ok ? 0 : 1;
}
