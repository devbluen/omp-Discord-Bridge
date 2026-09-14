/*
	Exemplo 09 - Moderação de membros

	Um comando /mod com subcomandos:
	  /mod expulsar  membro [motivo]
	  /mod banir     membro [motivo] [apagar_dias]
	  /mod desbanir  id [motivo]
	  /mod silenciar membro minutos [motivo]   (timeout)
	  /mod apelido   membro [apelido]          (vazio remove o apelido)
	  /mod cargo     membro cargo adicionar
	  /mod voz       membro                    (desconecta da call)

	Pontos importantes:
	  - o bot precisa das permissões no Discord (expulsar, banir, moderar
	    membros, gerenciar apelidos/cargos) e o cargo dele precisa estar
	    ACIMA do cargo de quem ele vai punir;
	  - os motivos aparecem no registro de auditoria do servidor;
	  - se o Discord recusar, DBR_OnActionFail avisa o motivo.
*/

#include <open.mp>
#include <discord-bridge>

#define CANAL_LOGS "123456789012345678"

main()
{
}

public OnGameModeInit()
{
	new DiscordCommand:mod = DBR_CreateCommand("mod", "Ferramentas de moderação");
	// Só quem pode moderar membros vê o comando.
	DBR_SetCommandPermissions(mod, DISCORD_COMMAND_MODERATORS);

	new DiscordCommandOption:sub;

	sub = DBR_AddCommandOption(mod, DISCORD_OPTION_SUBCOMMAND, "expulsar", "Expulsa um membro");
	DBR_AddCommandOption(mod, DISCORD_OPTION_USER, "membro", "Quem será expulso", true, sub);
	DBR_AddCommandOption(mod, DISCORD_OPTION_STRING, "motivo", "Motivo", false, sub);

	sub = DBR_AddCommandOption(mod, DISCORD_OPTION_SUBCOMMAND, "banir", "Bane um membro");
	DBR_AddCommandOption(mod, DISCORD_OPTION_USER, "membro", "Quem será banido", true, sub);
	DBR_AddCommandOption(mod, DISCORD_OPTION_STRING, "motivo", "Motivo", false, sub);
	new DiscordCommandOption:dias = DBR_AddCommandOption(mod, DISCORD_OPTION_INTEGER, "apagar_dias", "Apagar mensagens dos últimos N dias", false, sub);
	DBR_SetOptionRange(dias, 0.0, 7.0);

	sub = DBR_AddCommandOption(mod, DISCORD_OPTION_SUBCOMMAND, "desbanir", "Remove um banimento");
	DBR_AddCommandOption(mod, DISCORD_OPTION_STRING, "id", "ID do usuário banido", true, sub);
	DBR_AddCommandOption(mod, DISCORD_OPTION_STRING, "motivo", "Motivo", false, sub);

	sub = DBR_AddCommandOption(mod, DISCORD_OPTION_SUBCOMMAND, "silenciar", "Aplica um castigo (timeout)");
	DBR_AddCommandOption(mod, DISCORD_OPTION_USER, "membro", "Quem será silenciado", true, sub);
	new DiscordCommandOption:minutos = DBR_AddCommandOption(mod, DISCORD_OPTION_INTEGER, "minutos", "Duração (0 remove o castigo)", true, sub);
	DBR_SetOptionRange(minutos, 0.0, 40320.0);
	DBR_AddCommandOption(mod, DISCORD_OPTION_STRING, "motivo", "Motivo", false, sub);

	sub = DBR_AddCommandOption(mod, DISCORD_OPTION_SUBCOMMAND, "apelido", "Muda o apelido de um membro");
	DBR_AddCommandOption(mod, DISCORD_OPTION_USER, "membro", "Membro", true, sub);
	DBR_AddCommandOption(mod, DISCORD_OPTION_STRING, "apelido", "Novo apelido (vazio remove)", false, sub);

	sub = DBR_AddCommandOption(mod, DISCORD_OPTION_SUBCOMMAND, "cargo", "Adiciona ou remove um cargo");
	DBR_AddCommandOption(mod, DISCORD_OPTION_USER, "membro", "Membro", true, sub);
	DBR_AddCommandOption(mod, DISCORD_OPTION_ROLE, "cargo", "Cargo", true, sub);
	DBR_AddCommandOption(mod, DISCORD_OPTION_BOOLEAN, "adicionar", "Verdadeiro adiciona, falso remove", true, sub);

	sub = DBR_AddCommandOption(mod, DISCORD_OPTION_SUBCOMMAND, "voz", "Desconecta um membro da call");
	DBR_AddCommandOption(mod, DISCORD_OPTION_USER, "membro", "Membro", true, sub);
	return 1;
}

public DBR_OnCommand(DiscordInteraction:interaction, DiscordUser:user, const command[])
{
	if (strcmp(command, "mod"))
	{
		return 1;
	}

	new subcomando[DISCORD_COMMAND_NAME_SIZE], motivo[128];
	new DiscordGuild:servidor, DiscordUser:membro;
	DBR_GetInteractionSubcommand(interaction, subcomando);
	DBR_GetInteractionGuild(interaction, servidor);
	DBR_GetInteractionOptionUser(interaction, "membro", membro);
	if (!DBR_GetInteractionOptionString(interaction, "motivo", motivo))
	{
		motivo = "Sem motivo informado";
	}

	if (!strcmp(subcomando, "expulsar"))
	{
		// Checagem extra além da permissão do comando.
		if (!DBR_HasInteractionPermission(interaction, DISCORD_PERMISSION_KICK_MEMBERS))
		{
			return DBR_RespondInteraction(interaction, "Você não pode expulsar membros.", .ephemeral = true);
		}
		DBR_KickGuildMember(servidor, membro, motivo);
		RegistrarLog(user, "expulsou", membro, motivo);
		return DBR_RespondInteraction(interaction, "Membro expulso.", .ephemeral = true);
	}
	if (!strcmp(subcomando, "banir"))
	{
		if (!DBR_HasInteractionPermission(interaction, DISCORD_PERMISSION_BAN_MEMBERS))
		{
			return DBR_RespondInteraction(interaction, "Você não pode banir membros.", .ephemeral = true);
		}
		new dias;
		DBR_GetInteractionOptionInt(interaction, "apagar_dias", dias);
		DBR_BanGuildMember(servidor, membro, motivo, dias * 86400);
		RegistrarLog(user, "baniu", membro, motivo);
		return DBR_RespondInteraction(interaction, "Membro banido.", .ephemeral = true);
	}
	if (!strcmp(subcomando, "desbanir"))
	{
		// Quem está banido não está mais no servidor: usamos o ID direto.
		new id[DISCORD_ID_SIZE];
		DBR_GetInteractionOptionString(interaction, "id", id);
		new DiscordUser:banido = DBR_FindUserByID(id);
		if (banido == DISCORD_INVALID_USER)
		{
			return DBR_RespondInteraction(interaction, "ID inválido.", .ephemeral = true);
		}
		DBR_UnbanGuildMember(servidor, banido, motivo);
		return DBR_RespondInteraction(interaction, "Banimento removido.", .ephemeral = true);
	}
	if (!strcmp(subcomando, "silenciar"))
	{
		new minutos;
		DBR_GetInteractionOptionInt(interaction, "minutos", minutos);
		DBR_SetGuildMemberTimeout(servidor, membro, minutos * 60, motivo);
		RegistrarLog(user, minutos ? "silenciou" : "removeu o castigo de", membro, motivo);
		return DBR_RespondInteraction(interaction, minutos ? "Membro silenciado." : "Castigo removido.", .ephemeral = true);
	}
	if (!strcmp(subcomando, "apelido"))
	{
		new apelido[DISCORD_NICKNAME_SIZE];
		DBR_GetInteractionOptionString(interaction, "apelido", apelido);
		DBR_SetGuildMemberNickname(servidor, membro, apelido, "Alterado por comando");
		return DBR_RespondInteraction(interaction, "Apelido atualizado.", .ephemeral = true);
	}
	if (!strcmp(subcomando, "cargo"))
	{
		new DiscordRole:cargo, bool:adicionar;
		DBR_GetInteractionOptionRole(interaction, "cargo", cargo);
		DBR_GetInteractionOptionBool(interaction, "adicionar", adicionar);
		if (adicionar)
		{
			DBR_AddGuildMemberRole(servidor, membro, cargo, "Cargo dado por comando");
		}
		else
		{
			DBR_RemoveGuildMemberRole(servidor, membro, cargo, "Cargo removido por comando");
		}
		return DBR_RespondInteraction(interaction, "Cargos atualizados.", .ephemeral = true);
	}
	if (!strcmp(subcomando, "voz"))
	{
		DBR_DisconnectGuildMemberVoice(servidor, membro, "Desconectado por comando");
		return DBR_RespondInteraction(interaction, "Membro desconectado da call.", .ephemeral = true);
	}
	return 1;
}

RegistrarLog(DiscordUser:moderador, const acao[], DiscordUser:alvo, const motivo[])
{
	new nomeModerador[DISCORD_USERNAME_SIZE], nomeAlvo[DISCORD_USERNAME_SIZE], texto[256];
	DBR_GetUserName(moderador, nomeModerador);
	DBR_GetUserName(alvo, nomeAlvo);
	format(texto, sizeof texto, ":shield: **%s** %s **%s**. Motivo: %s", nomeModerador, acao, nomeAlvo, motivo);
	DBR_SendChannelMessage(DBR_FindChannelByID(CANAL_LOGS), texto);
}

public DBR_OnActionFail(const action[], http_status, error_code, const message[])
{
	// Código 50013 = o bot não tem permissão (ou o cargo dele está abaixo do alvo).
	if (error_code == 50013)
	{
		printf("[discord] %s: o bot não tem permissão para isso.", action);
		return 1;
	}
	printf("[discord] %s falhou (HTTP %d, código %d): %s", action, http_status, error_code, message);
	return 1;
}
