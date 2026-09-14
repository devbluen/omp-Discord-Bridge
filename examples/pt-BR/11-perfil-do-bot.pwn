/*
	Exemplo 11 - Perfil do bot

	O que você aprende aqui:
	  - status (online, ausente, não perturbe, invisível);
	  - atividade ("Jogando", "Assistindo", "Ouvindo", "Competindo", status
	    personalizado e "Transmitindo" com link);
	  - atividade que muda sozinha mostrando os jogadores online;
	  - trocar nome, avatar, banner, descrição e apelido por comando.

	Atenção: o Discord limita trocas de nome e avatar (poucas por hora).
	Status e atividade podem mudar à vontade.
*/

#include <open.mp>
#include <discord-bridge>

new gRodizio;

main()
{
}

public OnGameModeInit()
{
	new DiscordCommand:bot = DBR_CreateCommand("bot", "Configura o perfil do bot");
	DBR_SetCommandPermissions(bot, DISCORD_COMMAND_ADMINISTRATORS);

	new DiscordCommandOption:sub;

	sub = DBR_AddCommandOption(bot, DISCORD_OPTION_SUBCOMMAND, "nome", "Troca o nome do bot");
	DBR_AddCommandOption(bot, DISCORD_OPTION_STRING, "valor", "Novo nome", true, sub);

	// O arquivo é procurado na pasta do servidor e em scriptfiles/.
	sub = DBR_AddCommandOption(bot, DISCORD_OPTION_SUBCOMMAND, "avatar", "Troca o avatar (arquivo .png/.jpg/.gif)");
	DBR_AddCommandOption(bot, DISCORD_OPTION_STRING, "arquivo", "Ex.: bot/avatar.png", true, sub);

	sub = DBR_AddCommandOption(bot, DISCORD_OPTION_SUBCOMMAND, "banner", "Troca o banner (arquivo)");
	DBR_AddCommandOption(bot, DISCORD_OPTION_STRING, "arquivo", "Ex.: bot/banner.png", true, sub);

	sub = DBR_AddCommandOption(bot, DISCORD_OPTION_SUBCOMMAND, "descricao", "Troca o 'sobre mim' do bot");
	DBR_AddCommandOption(bot, DISCORD_OPTION_STRING, "valor", "Nova descrição", true, sub);

	sub = DBR_AddCommandOption(bot, DISCORD_OPTION_SUBCOMMAND, "apelido", "Apelido do bot neste servidor");
	DBR_AddCommandOption(bot, DISCORD_OPTION_STRING, "valor", "Novo apelido (vazio remove)", false, sub);
	return 1;
}

public DBR_OnReady()
{
	// Quem é o bot?
	new nome[DISCORD_USERNAME_SIZE];
	DBR_GetUserName(DBR_GetBotUser(), nome);
	printf("[discord] Conectado como %s", nome);

	DBR_SetBotPresenceStatus(DISCORD_BOT_PRESENCE_ONLINE);
	DBR_SetBotActivity("San Andreas Multiplayer", DISCORD_ACTIVITY_PLAYING);

	// Troca a atividade a cada 30 segundos.
	SetTimer("TrocarAtividade", 30000, true);
	return 1;
}

forward TrocarAtividade();
public TrocarAtividade()
{
	new online, texto[64];
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i))
		{
			online++;
		}
	}

	switch (gRodizio++ % 5)
	{
		case 0:
		{
			format(texto, sizeof texto, "%d jogadores online", online);
			DBR_SetBotActivity(texto, DISCORD_ACTIVITY_WATCHING);         // "Assistindo ..."
		}
		case 1: DBR_SetBotActivity("a rádio da cidade", DISCORD_ACTIVITY_LISTENING);  // "Ouvindo ..."
		case 2: DBR_SetBotActivity("o campeonato de corrida", DISCORD_ACTIVITY_COMPETING);
		case 3: DBR_SetBotActivity("Aceitando novos jogadores!", DISCORD_ACTIVITY_CUSTOM); // status personalizado
		case 4: DBR_SetBotActivity("evento ao vivo", DISCORD_ACTIVITY_STREAMING, "https://twitch.tv/openmp");
	}
	return 1;
}

public DBR_OnCommand(DiscordInteraction:interaction, DiscordUser:user, const command[])
{
	#pragma unused user
	if (strcmp(command, "bot"))
	{
		return 1;
	}

	new subcomando[DISCORD_COMMAND_NAME_SIZE], valor[400];
	DBR_GetInteractionSubcommand(interaction, subcomando);

	if (!strcmp(subcomando, "nome"))
	{
		DBR_GetInteractionOptionString(interaction, "valor", valor);
		DBR_SetBotUsername(valor);
	}
	else if (!strcmp(subcomando, "avatar"))
	{
		DBR_GetInteractionOptionString(interaction, "arquivo", valor);
		DBR_SetBotAvatar(valor);
	}
	else if (!strcmp(subcomando, "banner"))
	{
		DBR_GetInteractionOptionString(interaction, "arquivo", valor);
		DBR_SetBotBanner(valor);
	}
	else if (!strcmp(subcomando, "descricao"))
	{
		DBR_GetInteractionOptionString(interaction, "valor", valor);
		DBR_SetBotDescription(valor);
	}
	else if (!strcmp(subcomando, "apelido"))
	{
		new DiscordGuild:servidor;
		DBR_GetInteractionGuild(interaction, servidor);
		DBR_GetInteractionOptionString(interaction, "valor", valor);
		DBR_SetBotNickname(servidor, valor);
	}
	// Se o Discord recusar (arquivo não encontrado, limite de trocas...),
	// DBR_OnActionFail recebe o motivo.
	return DBR_RespondInteraction(interaction, "Pedido enviado ao Discord.", .ephemeral = true);
}

public DBR_OnActionFail(const action[], http_status, error_code, const message[])
{
	printf("[discord] %s falhou (HTTP %d, código %d): %s", action, http_status, error_code, message);
	return 1;
}
