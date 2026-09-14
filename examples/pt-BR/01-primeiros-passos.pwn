/*
	Exemplo 01 - Primeiros passos

	O que você aprende aqui:
	  - como saber que o bot conectou (DBR_OnReady);
	  - como enviar uma mensagem em um canal;
	  - como usar um callback para saber se a mensagem foi enviada;
	  - como ler mensagens do chat e responder (!ping);
	  - como descobrir por que algo falhou (DBR_OnActionFail).

	Antes de rodar:
	  1. Configure o token do bot (veja o README principal).
	  2. Troque CANAL_GERAL pelo ID de um canal do seu servidor Discord
	     (Discord > Configurações > Avançado > Modo desenvolvedor, depois
	     clique com o botão direito no canal > Copiar ID).
	  3. Para ler o texto das mensagens, ative o intent "Message Content"
	     no Discord Developer Portal.
*/

#include <open.mp>
#include <discord-bridge>

#define CANAL_GERAL "123456789012345678"

new DiscordChannel:gCanalGeral = DISCORD_INVALID_CHANNEL;

main()
{
}

public OnGameModeInit()
{
	// Um handle de canal pode ser criado a partir do ID a qualquer momento,
	// mesmo antes de o bot terminar de conectar.
	gCanalGeral = DBR_FindChannelByID(CANAL_GERAL);
	return 1;
}

// Chamado uma vez, quando o bot está conectado e os servidores foram carregados.
public DBR_OnReady()
{
	print("[discord] Bot conectado!");

	// Mensagem simples: texto e pronto.
	DBR_SendChannelMessage(gCanalGeral, "O servidor acabou de ligar! :tada:");
	return 1;
}

public OnPlayerConnect(playerid)
{
	new nome[MAX_PLAYER_NAME + 1], texto[96];
	GetPlayerName(playerid, nome, sizeof nome);
	format(texto, sizeof texto, "**%s** entrou no servidor.", nome);

	// Mensagem com callback: quando o Discord responder, "AoAvisarEntrada" é
	// chamada. O primeiro parâmetro é sempre a mensagem criada; depois vêm os
	// seus próprios valores, descritos no formato ("i" = um número inteiro).
	DBR_SendChannelMessage(gCanalGeral, texto, "AoAvisarEntrada", "i", playerid);
	return 1;
}

forward AoAvisarEntrada(DiscordMessage:mensagem, playerid);
public AoAvisarEntrada(DiscordMessage:mensagem, playerid)
{
	// DISCORD_INVALID_MESSAGE significa que o envio falhou.
	if (mensagem == DISCORD_INVALID_MESSAGE)
	{
		printf("[discord] Não foi possível avisar a entrada do jogador %d.", playerid);
		return 1;
	}

	new id[DISCORD_ID_SIZE];
	DBR_GetMessageID(mensagem, id);
	printf("[discord] Entrada do jogador %d avisada (mensagem %s).", playerid, id);
	return 1;
}

// Chamado para cada mensagem nova em qualquer canal que o bot enxerga.
public DBR_OnMessageCreate(DiscordMessage:message)
{
	// Ignora mensagens de bots, inclusive as do próprio bot.
	new DiscordUser:autor, bool:ehBot;
	DBR_GetMessageAuthor(message, autor);
	DBR_IsUserBot(autor, ehBot);
	if (ehBot)
	{
		return 1;
	}

	new conteudo[128];
	DBR_GetMessageContent(message, conteudo);
	if (conteudo[0] != '!')
	{
		return 1;
	}

	if (!strcmp(conteudo, "!ping", true))
	{
		DBR_ReplyMessage(message, "Pong! :ping_pong:");
	}
	else if (!strcmp(conteudo, "!jogadores", true))
	{
		new resposta[64], online;
		for (new i = 0; i < MAX_PLAYERS; i++)
		{
			if (IsPlayerConnected(i))
			{
				online++;
			}
		}
		format(resposta, sizeof resposta, "Temos **%d** jogador(es) online.", online);
		DBR_ReplyMessage(message, resposta);
	}
	return 1;
}

// Toda ação que o Discord recusar cai aqui: falta de permissão, canal
// inexistente, limite de requisições...
public DBR_OnActionFail(const action[], http_status, error_code, const message[])
{
	printf("[discord] %s falhou (HTTP %d, código %d): %s", action, http_status, error_code, message);
	return 1;
}
