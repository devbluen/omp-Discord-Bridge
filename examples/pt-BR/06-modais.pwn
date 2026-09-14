/*
	Exemplo 06 - Modais (formulários)

	O que você aprende aqui:
	  - abrir um modal a partir de um comando e a partir de um botão;
	  - campos de texto curtos e longos, obrigatórios ou não;
	  - um menu de seleção dentro do modal;
	  - ler as respostas em DBR_OnModalSubmit.

	Regras dos modais:
	  - só podem ser abertos como PRIMEIRA resposta de um comando ou botão;
	  - têm até 5 componentes;
	  - DBR_ShowModal consome o modal (não é preciso destruí-lo).
*/

#include <open.mp>
#include <discord-bridge>

#define CANAL_FORMULARIO "123456789012345678"
#define CANAL_STAFF      "123456789012345678"

main()
{
}

public OnGameModeInit()
{
	DBR_CreateCommand("whitelist", "Envia o pedido de whitelist", .callback = "Cmd_Whitelist");
	return 1;
}

public DBR_OnReady()
{
	// Uma mensagem fixa com um botão que abre o mesmo formulário.
	new DiscordComponent:linha = DBR_CreateActionRow();
	DBR_AddComponent(linha, DBR_CreateButton(DISCORD_BUTTON_PRIMARY, "Pedir whitelist", "abrir_whitelist"));

	new DiscordMessageBuilder:mensagem = DBR_CreateMessageBuilder("Quer jogar no servidor? Clique no botão e preencha o formulário.");
	DBR_AddBuilderComponent(mensagem, linha);
	DBR_SendMessage(DBR_FindChannelByID(CANAL_FORMULARIO), mensagem);
	return 1;
}

AbrirFormulario(DiscordInteraction:interaction)
{
	// O custom id "whitelist" identifica o modal quando ele for enviado.
	new DiscordModal:modal = DBR_CreateModal("whitelist", "Pedido de whitelist");

	// Texto informativo no topo.
	DBR_AddModalComponent(modal, DBR_CreateTextDisplay("Responda com atenção. A staff analisa em até 24h."));

	// Campo curto e obrigatório.
	DBR_AddModalTextInput(modal, "nick", "Nick no jogo", .placeholder = "Ex.: Carl_Johnson", .min_length = 3, .max_length = MAX_PLAYER_NAME);

	// Campo longo (parágrafo).
	DBR_AddModalTextInput(modal, "historia", "História do personagem", DISCORD_TEXT_INPUT_PARAGRAPH, .min_length = 50, .max_length = 1000, .description = "Conte de onde ele veio e o que busca.");

	// Campo opcional.
	DBR_AddModalTextInput(modal, "indicacao", "Quem te indicou?", .required = false);

	// Menu de seleção dentro do modal: embrulhe-o em um label.
	new DiscordComponent:faccao = DBR_CreateSelectMenu(DISCORD_SELECT_STRING, "faccao", "Escolha uma facção");
	DBR_AddSelectMenuOption(faccao, "Polícia", "policia");
	DBR_AddSelectMenuOption(faccao, "Médicos", "samu");
	DBR_AddSelectMenuOption(faccao, "Civil", "civil");
	DBR_AddModalComponent(modal, DBR_CreateLabel("Facção desejada", faccao));

	return DBR_ShowModal(interaction, modal);
}

forward Cmd_Whitelist(DiscordInteraction:interaction, DiscordUser:user);
public Cmd_Whitelist(DiscordInteraction:interaction, DiscordUser:user)
{
	#pragma unused user
	return AbrirFormulario(interaction);
}

public DBR_OnButton(DiscordInteraction:interaction, DiscordUser:user, const custom_id[])
{
	#pragma unused user
	if (!strcmp(custom_id, "abrir_whitelist"))
	{
		return AbrirFormulario(interaction);
	}
	return 1;
}

public DBR_OnModalSubmit(DiscordInteraction:interaction, DiscordUser:user, const custom_id[])
{
	if (strcmp(custom_id, "whitelist"))
	{
		return 1;
	}

	// Cada campo é lido pelo custom id que você deu a ele.
	new nick[MAX_PLAYER_NAME + 1], historia[1001], indicacao[64], faccao[16];
	DBR_GetInteractionValue(interaction, "nick", nick);
	DBR_GetInteractionValue(interaction, "historia", historia);
	DBR_GetInteractionValue(interaction, "faccao", faccao);
	if (!DBR_GetInteractionValue(interaction, "indicacao", indicacao) || indicacao[0] == '\0')
	{
		indicacao = "Ninguém";
	}

	new autor[DISCORD_USERNAME_SIZE];
	DBR_GetUserName(user, autor);

	// Manda o pedido para a staff.
	new DiscordEmbed:embed = DBR_CreateEmbed("Novo pedido de whitelist", historia, .colour = 0xFEE75C);
	DBR_AddEmbedField(embed, "Nick", nick, true);
	DBR_AddEmbedField(embed, "Facção", faccao, true);
	DBR_AddEmbedField(embed, "Indicação", indicacao, true);
	DBR_AddEmbedField(embed, "Discord", autor, true);
	DBR_SendChannelEmbedMessage(DBR_FindChannelByID(CANAL_STAFF), embed);

	// E confirma para a pessoa.
	return DBR_RespondInteraction(interaction, "Pedido enviado! Aguarde a resposta da staff.", .ephemeral = true);
}
