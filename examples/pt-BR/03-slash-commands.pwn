/*
	Exemplo 03 - Slash commands (comandos com /)

	O que você aprende aqui:
	  1. comando simples com um callback só dele;
	  2. opções (parâmetros) com tipos e limites;
	  3. escolhas fixas (choices) e permissões;
	  4. subcomandos e grupos (/conta ver, /conta senha trocar);
	  5. autocomplete (sugestões enquanto a pessoa digita);
	  6. comando registrado em um único servidor;
	  7. comandos de menu de contexto (botão direito em um usuário/mensagem).

	Como funciona o registro:
	  - crie os comandos quando quiser (aqui, no OnGameModeInit);
	  - o plugin publica tudo no Discord sozinho quando o bot fica pronto;
	  - comandos globais podem levar alguns minutos para aparecer. Durante
	    os testes, registre no seu servidor (item 6), que aparece na hora.
*/

#include <open.mp>
#include <discord-bridge>

#define SERVIDOR_ID "123456789012345678"

static const gVeiculos[][] =
{
	"Infernus", "Turismo", "Sultan", "Elegy", "NRG-500", "Sanchez", "Hydra", "Maverick"
};

main()
{
}

public OnGameModeInit()
{
	// 1) Comando simples. O callback recebe (interaction, user).
	DBR_CreateCommand("ping", "Responde com pong", .callback = "Cmd_Ping");

	// 2) Opções. O último parâmetro de DBR_AddCommandOption diz se é obrigatória.
	new DiscordCommand:dado = DBR_CreateCommand("dado", "Rola um dado", .callback = "Cmd_Dado");
	new DiscordCommandOption:lados = DBR_AddCommandOption(dado, DISCORD_OPTION_INTEGER, "lados", "Quantidade de lados (padrão 6)");
	DBR_SetOptionRange(lados, 2.0, 100.0);

	// 3) Escolhas fixas e permissão: só administradores veem /clima.
	new DiscordCommand:clima = DBR_CreateCommand("clima", "Muda o clima do servidor", .callback = "Cmd_Clima");
	DBR_SetCommandPermissions(clima, DISCORD_COMMAND_ADMINISTRATORS);
	new DiscordCommandOption:tipo = DBR_AddCommandOption(clima, DISCORD_OPTION_INTEGER, "tipo", "Tipo de clima", true);
	DBR_AddOptionChoiceInt(tipo, "Ensolarado", 1);
	DBR_AddOptionChoiceInt(tipo, "Chuvoso", 8);
	DBR_AddOptionChoiceInt(tipo, "Neblina", 9);

	// 4) Subcomandos e grupos. Sem callback: caem em DBR_OnCommand.
	new DiscordCommand:conta = DBR_CreateCommand("conta", "Gerencia a sua conta");

	new DiscordCommandOption:ver = DBR_AddCommandOption(conta, DISCORD_OPTION_SUBCOMMAND, "ver", "Mostra os dados de uma conta");
	DBR_AddCommandOption(conta, DISCORD_OPTION_STRING, "nick", "Nick no jogo", true, ver);

	new DiscordCommandOption:senha = DBR_AddCommandOption(conta, DISCORD_OPTION_SUBCOMMAND_GROUP, "senha", "Senha da conta");
	new DiscordCommandOption:trocar = DBR_AddCommandOption(conta, DISCORD_OPTION_SUBCOMMAND, "trocar", "Troca a senha", .parent = senha);
	new DiscordCommandOption:nova = DBR_AddCommandOption(conta, DISCORD_OPTION_STRING, "nova", "Nova senha", true, trocar);
	DBR_SetOptionLength(nova, 6, 32);

	// 5) Autocomplete: as sugestões são enviadas em DBR_OnAutocomplete.
	new DiscordCommand:veiculo = DBR_CreateCommand("veiculo", "Mostra informações de um veículo");
	new DiscordCommandOption:modelo = DBR_AddCommandOption(veiculo, DISCORD_OPTION_STRING, "modelo", "Nome do veículo", true);
	DBR_SetOptionAutocomplete(modelo);

	// 6) Comando apenas em um servidor (aparece imediatamente).
	DBR_CreateCommand("teste", "Comando de testes", DBR_FindGuildByID(SERVIDOR_ID), "Cmd_Teste");

	// 7) Menus de contexto: não têm descrição nem opções.
	DBR_CreateCommand("Ver ficha", .type = DISCORD_COMMAND_USER, .callback = "Ctx_VerFicha");
	DBR_CreateCommand("Denunciar mensagem", .type = DISCORD_COMMAND_MESSAGE, .callback = "Ctx_Denunciar");
	return 1;
}

// Compara textos com segurança: strcmp considera "" igual a qualquer texto.
stock bool:TextoIgual(const a[], const b[])
{
	return a[0] != '\0' && strcmp(a, b) == 0;
}

forward Cmd_Ping(DiscordInteraction:interaction, DiscordUser:user);
public Cmd_Ping(DiscordInteraction:interaction, DiscordUser:user)
{
	#pragma unused user
	return DBR_RespondInteraction(interaction, "Pong! :ping_pong:");
}

forward Cmd_Dado(DiscordInteraction:interaction, DiscordUser:user);
public Cmd_Dado(DiscordInteraction:interaction, DiscordUser:user)
{
	#pragma unused user
	// Se a opção não foi preenchida, a variável fica com o valor padrão.
	new lados = 6;
	DBR_GetInteractionOptionInt(interaction, "lados", lados);

	new resposta[64];
	format(resposta, sizeof resposta, ":game_die: Você tirou **%d** em um dado de %d lados.", random(lados) + 1, lados);
	return DBR_RespondInteraction(interaction, resposta);
}

forward Cmd_Clima(DiscordInteraction:interaction, DiscordUser:user);
public Cmd_Clima(DiscordInteraction:interaction, DiscordUser:user)
{
	#pragma unused user
	new tipo;
	DBR_GetInteractionOptionInt(interaction, "tipo", tipo);
	SetWeather(tipo);
	return DBR_RespondInteraction(interaction, "Clima alterado no servidor.", .ephemeral = true);
}

forward Cmd_Teste(DiscordInteraction:interaction, DiscordUser:user);
public Cmd_Teste(DiscordInteraction:interaction, DiscordUser:user)
{
	new nome[DISCORD_USERNAME_SIZE], resposta[96];
	DBR_GetUserName(user, nome);
	format(resposta, sizeof resposta, "Olá, %s! O comando de teste funcionou.", nome);
	return DBR_RespondInteraction(interaction, resposta, .ephemeral = true);
}

// Recebe os comandos que não têm callback próprio.
public DBR_OnCommand(DiscordInteraction:interaction, DiscordUser:user, const command[])
{
	#pragma unused user
	if (!strcmp(command, "conta"))
	{
		new grupo[DISCORD_COMMAND_NAME_SIZE], subcomando[DISCORD_COMMAND_NAME_SIZE];
		DBR_GetInteractionSubGroup(interaction, grupo);          // "" se não houver grupo
		DBR_GetInteractionSubcommand(interaction, subcomando);

		if (TextoIgual(subcomando, "ver"))
		{
			new nick[MAX_PLAYER_NAME + 1], resposta[96];
			DBR_GetInteractionOptionString(interaction, "nick", nick);
			format(resposta, sizeof resposta, "A conta **%s** está ativa.", nick);
			return DBR_RespondInteraction(interaction, resposta, .ephemeral = true);
		}
		if (TextoIgual(grupo, "senha") && TextoIgual(subcomando, "trocar"))
		{
			return DBR_RespondInteraction(interaction, "Senha alterada com sucesso.", .ephemeral = true);
		}
	}
	else if (!strcmp(command, "veiculo"))
	{
		new modelo[32], resposta[64];
		DBR_GetInteractionOptionString(interaction, "modelo", modelo);
		format(resposta, sizeof resposta, "Você escolheu o **%s**.", modelo);
		return DBR_RespondInteraction(interaction, resposta);
	}
	return 1;
}

// Chamado a cada letra digitada em uma opção com autocomplete.
public DBR_OnAutocomplete(DiscordInteraction:interaction, DiscordUser:user, const command[], const option[])
{
	#pragma unused user
	if (strcmp(command, "veiculo") || strcmp(option, "modelo"))
	{
		return 1;
	}

	new digitado[32];
	DBR_GetInteractionOptionString(interaction, option, digitado);
	for (new i = 0; i < sizeof gVeiculos; i++)
	{
		if (digitado[0] == '\0' || strfind(gVeiculos[i], digitado, true) != -1)
		{
			// Nome mostrado e valor enviado. Até 25 sugestões.
			DBR_AddAutocompleteChoice(interaction, gVeiculos[i], gVeiculos[i]);
		}
	}
	// As sugestões são enviadas automaticamente quando este callback termina.
	return 1;
}

forward Ctx_VerFicha(DiscordInteraction:interaction, DiscordUser:user);
public Ctx_VerFicha(DiscordInteraction:interaction, DiscordUser:user)
{
	#pragma unused user
	// O alvo é o usuário em que a pessoa clicou com o botão direito.
	new alvoId[DISCORD_ID_SIZE], nome[DISCORD_USERNAME_SIZE], resposta[96];
	DBR_GetInteractionTargetID(interaction, alvoId);
	DBR_GetUserName(DBR_FindUserByID(alvoId), nome);
	format(resposta, sizeof resposta, "Ficha de **%s**: nenhuma punição registrada.", nome);
	return DBR_RespondInteraction(interaction, resposta, .ephemeral = true);
}

forward Ctx_Denunciar(DiscordInteraction:interaction, DiscordUser:user);
public Ctx_Denunciar(DiscordInteraction:interaction, DiscordUser:user)
{
	#pragma unused user
	// Em comandos de mensagem, a mensagem alvo já vem carregada.
	new DiscordMessage:mensagem, conteudo[128], resposta[192];
	DBR_GetInteractionMessage(interaction, mensagem);
	DBR_GetMessageContent(mensagem, conteudo);
	format(resposta, sizeof resposta, "Denúncia registrada para a mensagem:\n> %s", conteudo);
	return DBR_RespondInteraction(interaction, resposta, .ephemeral = true);
}
