/*
	Exemplo 08 - Respondendo interações

	Uma "interação" é qualquer uso de comando, botão, menu ou modal. Todas
	precisam de resposta. Este exemplo mostra as formas de responder:

	  /simples    resposta comum, todos veem;
	  /privado    resposta que só quem usou vê (ephemeral);
	  /demorado   "pensando..." e resposta depois (DBR_DeferInteraction);
	  /seguimento resposta e depois outra mensagem (follow-up);
	  /editar     resposta que é editada depois;
	  /apagar     resposta que se apaga sozinha;
	  /completo   resposta com embed e botão (message builder).

	Também mostra DBR_OnInteraction, chamado antes de tudo, usado aqui para
	bloquear o uso fora do servidor (em mensagens diretas).

	Bom saber:
	  - se você não responder, o plugin confirma a interação sozinho ao final
	    do callback (mostra "pensando..." em comandos);
	  - o handle da interação vale por 15 minutos, então dá para responder
	    depois de um timer, de uma consulta ao banco de dados etc.;
	  - responder de novo não dá erro: vira uma mensagem de follow-up.
*/

#include <open.mp>
#include <discord-bridge>

main()
{
}

public OnGameModeInit()
{
	DBR_CreateCommand("simples", "Resposta comum");
	DBR_CreateCommand("privado", "Resposta que só você vê");
	DBR_CreateCommand("demorado", "Resposta depois de alguns segundos");
	DBR_CreateCommand("seguimento", "Resposta com mensagem extra");
	DBR_CreateCommand("editar", "Resposta que muda depois");
	DBR_CreateCommand("apagar", "Resposta que se apaga");
	DBR_CreateCommand("completo", "Resposta com embed e botão");
	return 1;
}

// Chamado antes de qualquer outro callback. Retorne 0 para parar por aqui.
public DBR_OnInteraction(DiscordInteraction:interaction, DiscordUser:user, DiscordInteractionType:type)
{
	#pragma unused user, type
	new DiscordGuild:servidor;
	DBR_GetInteractionGuild(interaction, servidor);
	if (servidor == DISCORD_INVALID_GUILD)
	{
		DBR_RespondInteraction(interaction, "Use os comandos dentro do servidor.", .ephemeral = true);
		return 0;
	}
	return 1;
}

public DBR_OnCommand(DiscordInteraction:interaction, DiscordUser:user, const command[])
{
	#pragma unused user
	if (!strcmp(command, "simples"))
	{
		return DBR_RespondInteraction(interaction, "Esta resposta todo mundo vê.");
	}
	if (!strcmp(command, "privado"))
	{
		return DBR_RespondInteraction(interaction, "Só você está vendo esta mensagem.", .ephemeral = true);
	}
	if (!strcmp(command, "demorado"))
	{
		// Mostra "pensando..." agora e responde em 3 segundos.
		DBR_DeferInteraction(interaction);
		SetTimerEx("ResponderDepois", 3000, false, "i", _:interaction);
		return 1;
	}
	if (!strcmp(command, "seguimento"))
	{
		DBR_RespondInteraction(interaction, "Primeira resposta.");
		// Um follow-up é enviado com um message builder.
		DBR_SendInteractionFollowup(interaction, DBR_CreateMessageBuilder("E esta é uma mensagem extra, só para você."), .ephemeral = true);
		return 1;
	}
	if (!strcmp(command, "editar"))
	{
		DBR_RespondInteraction(interaction, "Carregando dados...");
		SetTimerEx("EditarResposta", 2000, false, "i", _:interaction);
		return 1;
	}
	if (!strcmp(command, "apagar"))
	{
		DBR_RespondInteraction(interaction, "Esta mensagem some em 5 segundos.");
		SetTimerEx("ApagarResposta", 5000, false, "i", _:interaction);
		return 1;
	}
	if (!strcmp(command, "completo"))
	{
		new DiscordEmbed:embed = DBR_CreateEmbed("Resposta completa", "Com embed e botão.", .colour = 0x57F287);

		new DiscordComponent:linha = DBR_CreateActionRow();
		DBR_AddComponent(linha, DBR_CreateButton(DISCORD_BUTTON_LINK, "Documentação", "https://open.mp"));

		new DiscordMessageBuilder:mensagem = DBR_CreateMessageBuilder();
		DBR_AddBuilderEmbed(mensagem, embed);
		DBR_AddBuilderComponent(mensagem, linha);
		return DBR_RespondInteractionMessage(interaction, mensagem);
	}
	return 1;
}

forward ResponderDepois(DiscordInteraction:interaction);
public ResponderDepois(DiscordInteraction:interaction)
{
	// Depois de um defer, DBR_RespondInteraction substitui o "pensando...".
	return DBR_RespondInteraction(interaction, "Pronto! Terminei o processamento.");
}

forward EditarResposta(DiscordInteraction:interaction);
public EditarResposta(DiscordInteraction:interaction)
{
	return DBR_EditInteractionResponse(interaction, DBR_CreateMessageBuilder("Dados carregados: 42 jogadores cadastrados."));
}

forward ApagarResposta(DiscordInteraction:interaction);
public ApagarResposta(DiscordInteraction:interaction)
{
	return DBR_DeleteInteractionResponse(interaction);
}
