/*
	Exemplo 05 - Menus de seleção (select menus)

	O que você aprende aqui:
	  - menu com opções escritas por você, permitindo escolher várias;
	  - menus prontos do Discord: usuários, cargos e canais;
	  - ler os valores escolhidos em DBR_OnSelectMenu.

	Cada menu ocupa uma linha (action row) inteira.
*/

#include <open.mp>
#include <discord-bridge>

#define CANAL_MENUS "123456789012345678"

// Funções que retornam uma tag precisam ser declaradas antes do uso.
forward DiscordComponent:EmLinha(DiscordComponent:componente);

main()
{
}

public DBR_OnReady()
{
	new DiscordMessageBuilder:mensagem = DBR_CreateMessageBuilder("Escolha abaixo:");

	// 1) Opções próprias: escolher de 1 a 3 atividades.
	new DiscordComponent:atividades = DBR_CreateSelectMenu(DISCORD_SELECT_STRING, "atividades", "Do que você gosta no servidor?", 1, 3);
	//                                   rótulo         valor       descrição
	DBR_AddSelectMenuOption(atividades, "Corridas",    "corrida",  "Eventos de corrida");
	DBR_AddSelectMenuOption(atividades, "Roleplay",    "rp",       "Histórias e personagens");
	DBR_AddSelectMenuOption(atividades, "Tiroteio",    "tiro",     "Modos de combate");
	DBR_AddSelectMenuOption(atividades, "Construção",  "mapa",     "Mapas e objetos");
	DBR_AddBuilderComponent(mensagem, EmLinha(atividades));

	// 2) Usuários do servidor.
	DBR_AddBuilderComponent(mensagem, EmLinha(DBR_CreateSelectMenu(DISCORD_SELECT_USER, "amigo", "Indique um amigo")));

	// 3) Cargos do servidor.
	DBR_AddBuilderComponent(mensagem, EmLinha(DBR_CreateSelectMenu(DISCORD_SELECT_ROLE, "cargo", "Escolha um cargo")));

	// 4) Canais, filtrando só os de texto.
	new DiscordComponent:canais = DBR_CreateSelectMenu(DISCORD_SELECT_CHANNEL, "canal", "Onde receber avisos?");
	DBR_AddSelectMenuChannelType(canais, DISCORD_GUILD_TEXT);
	DBR_AddBuilderComponent(mensagem, EmLinha(canais));

	DBR_SendMessage(DBR_FindChannelByID(CANAL_MENUS), mensagem);
	return 1;
}

DiscordComponent:EmLinha(DiscordComponent:componente)
{
	new DiscordComponent:linha = DBR_CreateActionRow();
	DBR_AddComponent(linha, componente);
	return linha;
}

public DBR_OnSelectMenu(DiscordInteraction:interaction, DiscordUser:user, const custom_id[])
{
	#pragma unused user
	// Em menus, deixe o custom id vazio ("") para ler os valores escolhidos.
	new total = DBR_GetInteractionValueCount(interaction);
	new valor[DISCORD_ID_SIZE], resposta[256];

	if (!strcmp(custom_id, "atividades"))
	{
		resposta = "Você escolheu:";
		for (new i = 0; i < total; i++)
		{
			DBR_GetInteractionValue(interaction, "", valor, sizeof valor, i);
			format(resposta, sizeof resposta, "%s `%s`", resposta, valor);
		}
	}
	else if (!strcmp(custom_id, "amigo"))
	{
		// Menus de usuário devolvem IDs de usuário.
		new nome[DISCORD_USERNAME_SIZE];
		DBR_GetInteractionValue(interaction, "", valor);
		DBR_GetUserName(DBR_FindUserByID(valor), nome);
		format(resposta, sizeof resposta, "Obrigado por indicar **%s**!", nome);
	}
	else if (!strcmp(custom_id, "cargo"))
	{
		new nome[64];
		DBR_GetInteractionValue(interaction, "", valor);
		DBR_GetRoleName(DBR_FindRoleByID(valor), nome);
		format(resposta, sizeof resposta, "Cargo escolhido: **%s**.", nome);
	}
	else if (!strcmp(custom_id, "canal"))
	{
		DBR_GetInteractionValue(interaction, "", valor);
		format(resposta, sizeof resposta, "Os avisos irão para <#%s>.", valor);
	}
	else
	{
		return 1;
	}
	return DBR_RespondInteraction(interaction, resposta, .ephemeral = true);
}
