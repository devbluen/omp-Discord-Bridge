/*
	Exemplo 10 - Consultando servidor, membros, usuários, cargos e canais

	Existem dois jeitos de obter informações:

	  1. Getters (DBR_Get...): leem o que o plugin já tem em memória. São
	     instantâneos, mas retornam 0 se o dado ainda não foi carregado.

	  2. Fetch (DBR_Fetch...): buscam os dados atualizados no Discord. A
	     resposta chega em um callback; depois disso os getters funcionam.

	Comandos deste exemplo:
	  /servidor         dados do servidor (DBR_FetchGuild)
	  /perfil membro    perfil de um membro (DBR_FetchGuildMember)
	  /cargo cargo      dados de um cargo (DBR_FetchRole)
	  /canal canal      dados de um canal (DBR_FetchChannel)
	  /usuario id       qualquer usuário do Discord pelo ID (DBR_FetchUser)

	Também mostra uma mensagem de boas-vindas usando só getters.
*/

#include <open.mp>
#include <discord-bridge>

#define CANAL_BOAS_VINDAS "123456789012345678"

main()
{
}

public OnGameModeInit()
{
	DBR_CreateCommand("servidor", "Mostra os dados do servidor");

	new DiscordCommand:perfil = DBR_CreateCommand("perfil", "Mostra o perfil de um membro");
	DBR_AddCommandOption(perfil, DISCORD_OPTION_USER, "membro", "Membro", true);

	new DiscordCommand:cargo = DBR_CreateCommand("cargo", "Mostra os dados de um cargo");
	DBR_AddCommandOption(cargo, DISCORD_OPTION_ROLE, "cargo", "Cargo", true);

	new DiscordCommand:canal = DBR_CreateCommand("canal", "Mostra os dados de um canal");
	DBR_AddCommandOption(canal, DISCORD_OPTION_CHANNEL, "canal", "Canal", true);

	new DiscordCommand:usuario = DBR_CreateCommand("usuario", "Busca um usuário pelo ID");
	DBR_AddCommandOption(usuario, DISCORD_OPTION_STRING, "id", "ID do usuário", true);
	return 1;
}

public DBR_OnCommand(DiscordInteraction:interaction, DiscordUser:user, const command[])
{
	#pragma unused user
	// Todas as buscas levam um instante: mostramos "pensando..." e passamos a
	// interação para o callback responder ("i" = um inteiro).
	DBR_DeferInteraction(interaction, .ephemeral = true);

	if (!strcmp(command, "servidor"))
	{
		new DiscordGuild:servidor;
		DBR_GetInteractionGuild(interaction, servidor);
		return DBR_FetchGuild(servidor, "AoCarregarServidor", "i", _:interaction);
	}
	if (!strcmp(command, "perfil"))
	{
		new DiscordGuild:servidor, DiscordUser:membro;
		DBR_GetInteractionGuild(interaction, servidor);
		DBR_GetInteractionOptionUser(interaction, "membro", membro);
		return DBR_FetchGuildMember(servidor, membro, "AoCarregarMembro", "i", _:interaction);
	}
	if (!strcmp(command, "cargo"))
	{
		new DiscordGuild:servidor, DiscordRole:cargo;
		DBR_GetInteractionGuild(interaction, servidor);
		DBR_GetInteractionOptionRole(interaction, "cargo", cargo);
		return DBR_FetchRole(servidor, cargo, "AoCarregarCargo", "i", _:interaction);
	}
	if (!strcmp(command, "canal"))
	{
		new DiscordChannel:canal;
		DBR_GetInteractionOptionChannel(interaction, "canal", canal);
		return DBR_FetchChannel(canal, "AoCarregarCanal", "i", _:interaction);
	}
	if (!strcmp(command, "usuario"))
	{
		new id[DISCORD_ID_SIZE];
		DBR_GetInteractionOptionString(interaction, "id", id);
		new DiscordUser:alvo = DBR_FindUserByID(id);
		if (alvo == DISCORD_INVALID_USER)
		{
			return DBR_RespondInteraction(interaction, "Isso não parece um ID válido.");
		}
		return DBR_FetchUser(alvo, "AoCarregarUsuario", "i", _:interaction);
	}
	return 1;
}

// Callbacks de fetch: primeiro vem o que foi carregado (0 se falhou),
// depois os valores que você passou.

forward AoCarregarServidor(DiscordGuild:servidor, DiscordInteraction:interaction);
public AoCarregarServidor(DiscordGuild:servidor, DiscordInteraction:interaction)
{
	if (servidor == DISCORD_INVALID_GUILD)
	{
		return DBR_RespondInteraction(interaction, "Não consegui carregar o servidor.");
	}

	new nome[100], descricao[256], icone[DISCORD_URL_SIZE], dono[DISCORD_ID_SIZE], membros, texto[32];
	DBR_GetGuildName(servidor, nome);
	DBR_GetGuildDescription(servidor, descricao);
	DBR_GetGuildIconURL(servidor, icone);
	DBR_GetGuildOwnerID(servidor, dono);
	DBR_GetGuildTotalMembers(servidor, membros);

	new DiscordEmbed:embed = DBR_CreateEmbed(nome, descricao[0] ? descricao : "Sem descrição.", .colour = 0x5865F2);
	if (icone[0])
	{
		DBR_SetEmbedThumbnail(embed, icone);
	}
	valstr(texto, membros);
	DBR_AddEmbedField(embed, "Membros", texto, true);
	format(texto, sizeof texto, "<@%s>", dono);
	DBR_AddEmbedField(embed, "Dono", texto, true);
	return DBR_RespondInteractionEmbed(interaction, embed);
}

forward AoCarregarMembro(DiscordGuild:servidor, DiscordUser:membro, DiscordInteraction:interaction);
public AoCarregarMembro(DiscordGuild:servidor, DiscordUser:membro, DiscordInteraction:interaction)
{
	if (membro == DISCORD_INVALID_USER)
	{
		return DBR_RespondInteraction(interaction, "Esse usuário não está no servidor.");
	}

	new nome[DISCORD_USERNAME_SIZE], usuario[DISCORD_USERNAME_SIZE], entrou[DISCORD_TIMESTAMP_SIZE], avatar[DISCORD_URL_SIZE];
	new cargos, castigo, texto[32], bool:ehBot;
	DBR_GetGuildMemberDisplayName(servidor, membro, nome);   // apelido > nome global > usuário
	DBR_GetUserName(membro, usuario);
	DBR_GetGuildMemberJoinedAt(servidor, membro, entrou);
	DBR_GetGuildMemberAvatarURL(servidor, membro, avatar);
	DBR_GetGuildMemberRoleCount(servidor, membro, cargos);
	DBR_GetGuildMemberTimeout(servidor, membro, castigo);
	DBR_IsUserBot(membro, ehBot);

	new DiscordEmbed:embed = DBR_CreateEmbed(nome, .colour = 0x57F287);
	DBR_SetEmbedThumbnail(embed, avatar);
	DBR_AddEmbedField(embed, "Usuário", usuario, true);
	DBR_AddEmbedField(embed, "Bot", ehBot ? "Sim" : "Não", true);
	DBR_AddEmbedField(embed, "Entrou em", entrou, false);
	valstr(texto, cargos);
	DBR_AddEmbedField(embed, "Cargos", texto, true);
	format(texto, sizeof texto, "%d segundo(s)", castigo);
	DBR_AddEmbedField(embed, "Castigo restante", texto, true);
	return DBR_RespondInteractionEmbed(interaction, embed);
}

forward AoCarregarCargo(DiscordRole:cargo, DiscordInteraction:interaction);
public AoCarregarCargo(DiscordRole:cargo, DiscordInteraction:interaction)
{
	if (cargo == DISCORD_INVALID_ROLE)
	{
		return DBR_RespondInteraction(interaction, "Não consegui carregar o cargo.");
	}

	new nome[100], cor, posicao, bool:separado, bool:mencionavel, texto[128];
	DBR_GetRoleName(cargo, nome);
	DBR_GetRoleColour(cargo, cor);
	DBR_GetRolePosition(cargo, posicao);
	DBR_IsRoleHoist(cargo, separado);
	DBR_IsRoleMentionable(cargo, mencionavel);

	format(texto, sizeof texto, "**%s**\nCor: #%06x | Posição: %d | Separado: %s | Mencionável: %s",
		nome, cor, posicao, separado ? "sim" : "não", mencionavel ? "sim" : "não");
	return DBR_RespondInteraction(interaction, texto);
}

forward AoCarregarCanal(DiscordChannel:canal, DiscordInteraction:interaction);
public AoCarregarCanal(DiscordChannel:canal, DiscordInteraction:interaction)
{
	if (canal == DISCORD_INVALID_CHANNEL)
	{
		return DBR_RespondInteraction(interaction, "Não consegui carregar o canal.");
	}

	new nome[100], topico[256], modoLento, DiscordChannelType:tipo, texto[400];
	DBR_GetChannelName(canal, nome);
	DBR_GetChannelTopic(canal, topico);
	DBR_GetChannelSlowmode(canal, modoLento);
	DBR_GetChannelType(canal, tipo);

	format(texto, sizeof texto, "**#%s** (tipo %d)\nTópico: %s\nModo lento: %d segundo(s)",
		nome, _:tipo, topico[0] ? topico : "nenhum", modoLento);
	return DBR_RespondInteraction(interaction, texto);
}

forward AoCarregarUsuario(DiscordUser:alvo, DiscordInteraction:interaction);
public AoCarregarUsuario(DiscordUser:alvo, DiscordInteraction:interaction)
{
	if (alvo == DISCORD_INVALID_USER)
	{
		return DBR_RespondInteraction(interaction, "Usuário não encontrado.");
	}

	new usuario[DISCORD_USERNAME_SIZE], nomeGlobal[DISCORD_USERNAME_SIZE], avatar[DISCORD_URL_SIZE], banner[DISCORD_URL_SIZE];
	DBR_GetUserName(alvo, usuario);
	DBR_GetUserGlobalName(alvo, nomeGlobal);
	DBR_GetUserAvatarURL(alvo, avatar, .size = 512);

	new DiscordEmbed:embed = DBR_CreateEmbed(nomeGlobal[0] ? nomeGlobal : usuario, .colour = 0xEB459E);
	DBR_SetEmbedThumbnail(embed, avatar);
	DBR_AddEmbedField(embed, "Usuário", usuario, true);
	if (DBR_GetUserBannerURL(alvo, banner))
	{
		DBR_SetEmbedImage(embed, banner);
	}
	return DBR_RespondInteractionEmbed(interaction, embed);
}

// Getters funcionam direto em eventos, porque o membro acabou de chegar.
public DBR_OnGuildMemberAdd(DiscordGuild:guild, DiscordUser:user)
{
	new nome[DISCORD_USERNAME_SIZE], servidor[100], avatar[DISCORD_URL_SIZE], texto[160];
	DBR_GetGuildMemberDisplayName(guild, user, nome);
	DBR_GetGuildName(guild, servidor);
	DBR_GetUserAvatarURL(user, avatar);

	format(texto, sizeof texto, "Seja bem-vindo(a) ao **%s**, %s!", servidor, nome);
	new DiscordEmbed:embed = DBR_CreateEmbed("Novo membro", texto, .colour = 0x57F287);
	DBR_SetEmbedThumbnail(embed, avatar);
	DBR_SendChannelEmbedMessage(DBR_FindChannelByID(CANAL_BOAS_VINDAS), embed);
	return 1;
}
