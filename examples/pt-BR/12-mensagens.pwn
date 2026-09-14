/*
	Exemplo 12 - Gerenciando mensagens

	O que você aprende aqui:
	  - responder, reagir, editar e apagar mensagens;
	  - fixar mensagens pelo menu de contexto;
	  - apagar várias mensagens de uma vez (/limpar);
	  - apagar uma mensagem pelo ID;
	  - enviar mensagem direta (DM) para um usuário;
	  - acompanhar reações.

	Para ler o texto das mensagens, ative o intent "Message Content" no
	Discord Developer Portal.
*/

#include <open.mp>
#include <discord-bridge>

#define CANAL_AVISOS "123456789012345678"

main()
{
}

public OnGameModeInit()
{
	new DiscordCommand:limpar = DBR_CreateCommand("limpar", "Apaga mensagens recentes deste canal");
	DBR_SetCommandPermissions(limpar, DISCORD_COMMAND_MODERATORS);
	new DiscordCommandOption:quantidade = DBR_AddCommandOption(limpar, DISCORD_OPTION_INTEGER, "quantidade", "De 1 a 100", true);
	DBR_SetOptionRange(quantidade, 1.0, 100.0);

	new DiscordCommand:apagar = DBR_CreateCommand("apagar", "Apaga uma mensagem pelo ID");
	DBR_SetCommandPermissions(apagar, DISCORD_COMMAND_MODERATORS);
	DBR_AddCommandOption(apagar, DISCORD_OPTION_STRING, "id", "ID da mensagem", true);

	new DiscordCommand:dm = DBR_CreateCommand("dm", "Envia uma mensagem direta");
	DBR_SetCommandPermissions(dm, DISCORD_COMMAND_ADMINISTRATORS);
	DBR_AddCommandOption(dm, DISCORD_OPTION_USER, "usuario", "Quem recebe", true);
	DBR_AddCommandOption(dm, DISCORD_OPTION_STRING, "texto", "Mensagem", true);

	// Botão direito em uma mensagem > Apps > Fixar/Desafixar.
	DBR_CreateCommand("Fixar", .type = DISCORD_COMMAND_MESSAGE);
	DBR_CreateCommand("Desafixar", .type = DISCORD_COMMAND_MESSAGE);
	return 1;
}

public DBR_OnReady()
{
	// Envia um aviso, espera o Discord criar e depois edita.
	DBR_SendChannelMessage(DBR_FindChannelByID(CANAL_AVISOS), "Servidor iniciando...", "AoEnviarAviso");
	return 1;
}

forward AoEnviarAviso(DiscordMessage:mensagem);
public AoEnviarAviso(DiscordMessage:mensagem)
{
	if (mensagem != DISCORD_INVALID_MESSAGE)
	{
		SetTimerEx("FinalizarAviso", 5000, false, "i", _:mensagem);
	}
	return 1;
}

forward FinalizarAviso(DiscordMessage:mensagem);
public FinalizarAviso(DiscordMessage:mensagem)
{
	DBR_EditMessage(mensagem, "Servidor **online**! :white_check_mark:");
	return 1;
}

public DBR_OnMessageCreate(DiscordMessage:message)
{
	new DiscordUser:autor, bool:ehBot, conteudo[128];
	DBR_GetMessageAuthor(message, autor);
	DBR_IsUserBot(autor, ehBot);
	if (ehBot)
	{
		return 1;
	}

	DBR_GetMessageContent(message, conteudo);
	if (conteudo[0] == '\0')
	{
		return 1;
	}

	if (!strcmp(conteudo, "!oi", true))
	{
		// Responde citando a mensagem, sem notificar o autor.
		DBR_ReplyMessage(message, "Oi! Tudo bem?", .mention_author = false);
		// Reage com um emoji. Emojis Unicode funcionam com o arquivo salvo em
		// UTF-8; emojis do servidor usam DBR_CreateEmoji("nome", "id").
		DBR_CreateReaction(message, DBR_CreateEmoji("👋"));
	}
	else if (strfind(conteudo, "palavrão", true) != -1)
	{
		DBR_DeleteMessage(message);
	}
	return 1;
}

public DBR_OnCommand(DiscordInteraction:interaction, DiscordUser:user, const command[])
{
	#pragma unused user
	if (!strcmp(command, "limpar"))
	{
		new quantidade, DiscordChannel:canal;
		DBR_GetInteractionOptionInt(interaction, "quantidade", quantidade);
		DBR_GetInteractionChannel(interaction, canal);
		DBR_DeferInteraction(interaction, .ephemeral = true);
		// Mensagens com mais de 14 dias não podem ser apagadas em massa.
		return DBR_BulkDeleteMessages(canal, quantidade, "AoLimpar", "i", _:interaction);
	}
	if (!strcmp(command, "apagar"))
	{
		new id[DISCORD_ID_SIZE], DiscordChannel:canal;
		DBR_GetInteractionOptionString(interaction, "id", id);
		DBR_GetInteractionChannel(interaction, canal);
		DBR_DeleteMessageByID(canal, id, "Apagada por comando");
		return DBR_RespondInteraction(interaction, "Pedido de exclusão enviado.", .ephemeral = true);
	}
	if (!strcmp(command, "dm"))
	{
		new DiscordUser:destino, texto[1000];
		DBR_GetInteractionOptionUser(interaction, "usuario", destino);
		DBR_GetInteractionOptionString(interaction, "texto", texto);
		DBR_SendDirectMessage(destino, DBR_CreateMessageBuilder(texto), "AoEnviarDM", "i", _:interaction);
		return DBR_DeferInteraction(interaction, .ephemeral = true);
	}
	if (!strcmp(command, "Fixar") || !strcmp(command, "Desafixar"))
	{
		new DiscordMessage:alvo;
		DBR_GetInteractionMessage(interaction, alvo);
		if (!strcmp(command, "Fixar"))
		{
			DBR_PinMessage(alvo);
		}
		else
		{
			DBR_UnpinMessage(alvo);
		}
		return DBR_RespondInteraction(interaction, "Feito!", .ephemeral = true);
	}
	return 1;
}

forward AoLimpar(apagadas, DiscordInteraction:interaction);
public AoLimpar(apagadas, DiscordInteraction:interaction)
{
	new texto[64];
	format(texto, sizeof texto, ":broom: %d mensagem(ns) apagada(s).", apagadas);
	return DBR_RespondInteraction(interaction, texto);
}

forward AoEnviarDM(DiscordMessage:mensagem, DiscordInteraction:interaction);
public AoEnviarDM(DiscordMessage:mensagem, DiscordInteraction:interaction)
{
	// A DM falha quando a pessoa bloqueia mensagens de membros do servidor.
	if (mensagem == DISCORD_INVALID_MESSAGE)
	{
		return DBR_RespondInteraction(interaction, "Não foi possível enviar a DM.");
	}
	return DBR_RespondInteraction(interaction, "Mensagem enviada!");
}

public DBR_OnMessageReaction(DiscordMessage:message, DiscordUser:reaction_user, DiscordEmoji:emoji, DiscordMessageReactionType:reaction_type)
{
	#pragma unused message
	if (reaction_type != DISCORD_REACTION_ADD)
	{
		return 1;
	}
	new nome[DISCORD_USERNAME_SIZE], emojiNome[DISCORD_EMOJI_NAME_SIZE];
	DBR_GetUserName(reaction_user, nome);
	DBR_GetEmojiName(emoji, emojiNome);
	printf("[discord] %s reagiu com %s", nome, emojiNome);
	return 1;
}
