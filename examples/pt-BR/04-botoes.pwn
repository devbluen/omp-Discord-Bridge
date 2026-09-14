/*
	Exemplo 04 - Botões

	O que você aprende aqui:
	  - os estilos de botão (primário, secundário, sucesso, perigo e link);
	  - responder um clique em DBR_OnButton;
	  - usar um handler por prefixo do custom id ("contador:");
	  - atualizar a própria mensagem do botão (contador que aumenta);
	  - desabilitar um botão.

	O "custom id" é um texto que você escolhe (até 100 caracteres) e que o
	Discord devolve quando alguém clica. Use-o para saber qual botão foi usado
	e até para guardar dados, como o número do contador abaixo.
*/

#include <open.mp>
#include <discord-bridge>

#define CANAL_BOTOES "123456789012345678"
#define LIMITE_CONTADOR 10

// Funções que retornam uma tag precisam ser declaradas antes do uso.
forward DiscordComponent:CriarLinhaContador(valor);

main()
{
}

public OnGameModeInit()
{
	// Todo clique cujo custom id começa com "contador:" vai para AoClicarContador,
	// e não para DBR_OnButton.
	DBR_RegisterHandler(DISCORD_INTERACTION_COMPONENT, "contador:", "AoClicarContador", true);
	return 1;
}

public DBR_OnReady()
{
	// Botões ficam dentro de linhas (action rows). Cada linha comporta até 5.
	new DiscordComponent:linha = DBR_CreateActionRow();
	DBR_AddComponent(linha, DBR_CreateButton(DISCORD_BUTTON_PRIMARY, "Dizer olá", "ola"));
	DBR_AddComponent(linha, DBR_CreateButton(DISCORD_BUTTON_SECONDARY, "Ver horário", "horario"));
	DBR_AddComponent(linha, DBR_CreateButton(DISCORD_BUTTON_DANGER, "Apagar mensagem", "apagar"));
	// Botões de link recebem uma URL no lugar do custom id e não geram clique.
	DBR_AddComponent(linha, DBR_CreateButton(DISCORD_BUTTON_LINK, "Site do open.mp", "https://open.mp"));

	new DiscordMessageBuilder:mensagem = DBR_CreateMessageBuilder("Teste os botões abaixo:");
	DBR_AddBuilderComponent(mensagem, linha);
	DBR_AddBuilderComponent(mensagem, CriarLinhaContador(0));
	DBR_SendMessage(DBR_FindChannelByID(CANAL_BOTOES), mensagem);
	return 1;
}

DiscordComponent:CriarLinhaContador(valor)
{
	new rotulo[32], customId[DISCORD_CUSTOM_ID_SIZE];
	format(rotulo, sizeof rotulo, "Cliques: %d", valor);
	format(customId, sizeof customId, "contador:%d", valor);

	new DiscordComponent:botao = DBR_CreateButton(DISCORD_BUTTON_SUCCESS, rotulo, customId);
	if (valor >= LIMITE_CONTADOR)
	{
		DBR_SetComponentDisabled(botao, true);
	}

	new DiscordComponent:linha = DBR_CreateActionRow();
	DBR_AddComponent(linha, botao);
	return linha;
}

// Recebe os cliques que não foram capturados por um handler.
public DBR_OnButton(DiscordInteraction:interaction, DiscordUser:user, const custom_id[])
{
	if (!strcmp(custom_id, "ola"))
	{
		new nome[DISCORD_USERNAME_SIZE], resposta[64];
		DBR_GetUserName(user, nome);
		format(resposta, sizeof resposta, "Olá, %s! :wave:", nome);
		// ephemeral = só quem clicou vê a resposta.
		return DBR_RespondInteraction(interaction, resposta, .ephemeral = true);
	}
	if (!strcmp(custom_id, "horario"))
	{
		new hora, minuto, segundo, resposta[48];
		gettime(hora, minuto, segundo);
		format(resposta, sizeof resposta, "No servidor são %02d:%02d.", hora, minuto);
		return DBR_RespondInteraction(interaction, resposta, .ephemeral = true);
	}
	if (!strcmp(custom_id, "apagar"))
	{
		// A mensagem onde o botão está.
		new DiscordMessage:mensagem;
		DBR_GetInteractionMessage(interaction, mensagem);
		DBR_DeleteMessage(mensagem);
		// Sem resposta: o plugin confirma o clique sozinho para o Discord.
	}
	return 1;
}

forward AoClicarContador(DiscordInteraction:interaction, DiscordUser:user, const custom_id[]);
public AoClicarContador(DiscordInteraction:interaction, DiscordUser:user, const custom_id[])
{
	#pragma unused user
	// "contador:3" -> 3 (o número começa depois dos 9 caracteres do prefixo).
	new valor = strval(custom_id[9]) + 1;

	// Troca o conteúdo da mensagem em que o botão foi clicado.
	new DiscordMessageBuilder:mensagem = DBR_CreateMessageBuilder("O contador foi atualizado:");
	DBR_AddBuilderComponent(mensagem, CriarLinhaContador(valor));
	return DBR_UpdateInteractionMessage(interaction, mensagem);
}
