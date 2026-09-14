/*
	Exemplo 02 - Embeds

	O que você aprende aqui:
	  - montar uma embed com título, descrição, cor, autor, campos, imagens e rodapé;
	  - enviar uma embed com um texto junto;
	  - enviar várias embeds na mesma mensagem (message builder);
	  - manter um "painel de status" atualizado editando sempre a mesma mensagem.

	Regra importante: enviar uma embed CONSOME o handle. Depois de
	DBR_SendChannelEmbedMessage, DBR_AddBuilderEmbed ou DBR_EditMessage você não
	precisa (e não pode) usar DBR_DeleteEmbed nela.
*/

#include <open.mp>
#include <discord-bridge>

#define CANAL_STATUS "123456789012345678"

new DiscordChannel:gCanal = DISCORD_INVALID_CHANNEL;
new DiscordMessage:gPainelStatus = DISCORD_INVALID_MESSAGE;

// Funções que retornam uma tag precisam ser declaradas antes do uso.
forward DiscordEmbed:CriarEmbedStatus();

main()
{
}

public OnGameModeInit()
{
	gCanal = DBR_FindChannelByID(CANAL_STATUS);
	return 1;
}

public DBR_OnReady()
{
	EnviarEmbedCompleta();
	EnviarVariasEmbeds();

	// Cria o painel e guarda a mensagem para editar depois.
	DBR_SendChannelEmbedMessage(gCanal, CriarEmbedStatus(), "", "AoCriarPainel");
	return 1;
}

EnviarEmbedCompleta()
{
	// Todos os parâmetros de DBR_CreateEmbed são opcionais.
	new DiscordEmbed:embed = DBR_CreateEmbed("Bem-vindo ao Meu Servidor RP");

	DBR_SetEmbedDescription(embed, "Um servidor de roleplay com economia, empregos e muito mais.");
	DBR_SetEmbedColour(embed, 0x5865F2);                       // cor da barra lateral (RRGGBB)
	DBR_SetEmbedURL(embed, "https://open.mp");                  // o título vira um link
	DBR_SetEmbedAuthor(embed, "Equipe Meu Servidor", "https://open.mp", "https://assets.open.mp/assets/images/assets/logo-light-trans.png");

	// Campos: nome, valor e se ficam lado a lado (inline).
	DBR_AddEmbedField(embed, "IP", "`meuservidor.com:7777`", true);
	DBR_AddEmbedField(embed, "Versão", "open.mp", true);
	DBR_AddEmbedField(embed, "Regras", "Leia o canal #regras antes de jogar.", false);

	DBR_SetEmbedThumbnail(embed, "https://assets.open.mp/assets/images/assets/logo-light-trans.png");
	DBR_SetEmbedImage(embed, "https://assets.open.mp/assets/images/assets/logo-light-trans.png");
	DBR_SetEmbedFooter(embed, "Enviado automaticamente pelo servidor");
	DBR_SetEmbedTimestamp(embed, "2026-01-01T12:00:00Z");       // data no formato ISO 8601 (UTC)

	// O texto é opcional e aparece acima da embed.
	DBR_SendChannelEmbedMessage(gCanal, embed, "Confira as informações do servidor:");
}

EnviarVariasEmbeds()
{
	// Para mais de uma embed na mesma mensagem, use um message builder.
	new DiscordMessageBuilder:mensagem = DBR_CreateMessageBuilder("Horários de eventos desta semana:");
	DBR_AddBuilderEmbed(mensagem, DBR_CreateEmbed("Corrida", "Sábado às 20h", .colour = 0xFEE75C));
	DBR_AddBuilderEmbed(mensagem, DBR_CreateEmbed("Tiro ao alvo", "Domingo às 18h", .colour = 0xED4245));
	DBR_SendMessage(gCanal, mensagem);
}

DiscordEmbed:CriarEmbedStatus()
{
	new online, texto[32];
	for (new i = 0; i < MAX_PLAYERS; i++)
	{
		if (IsPlayerConnected(i))
		{
			online++;
		}
	}
	format(texto, sizeof texto, "%d/%d", online, MAX_PLAYERS);

	new DiscordEmbed:embed = DBR_CreateEmbed("Status do servidor", .colour = 0x57F287);
	DBR_AddEmbedField(embed, "Jogadores online", texto, true);
	DBR_AddEmbedField(embed, "Situação", "Online", true);
	DBR_SetEmbedFooter(embed, "Atualiza a cada minuto");
	return embed;
}

forward AoCriarPainel(DiscordMessage:mensagem);
public AoCriarPainel(DiscordMessage:mensagem)
{
	if (mensagem == DISCORD_INVALID_MESSAGE)
	{
		return 1;
	}
	// O handle continua válido para editar, apagar ou fixar a mensagem.
	gPainelStatus = mensagem;
	SetTimer("AtualizarPainel", 60000, true);
	return 1;
}

forward AtualizarPainel();
public AtualizarPainel()
{
	if (gPainelStatus != DISCORD_INVALID_MESSAGE)
	{
		// Edita a mesma mensagem: texto vazio e uma embed nova.
		DBR_EditMessage(gPainelStatus, "", CriarEmbedStatus());
	}
	return 1;
}
