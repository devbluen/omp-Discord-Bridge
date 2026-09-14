/*
	Exemplo 07 - Components V2

	Components V2 é o novo formato de mensagens do Discord: em vez de texto +
	embeds, a mensagem inteira é montada com blocos.

	Blocos disponíveis:
	  - Text Display  : texto com markdown (títulos, negrito, listas...);
	  - Separator     : linha/espaço entre blocos;
	  - Container     : caixa com barra colorida que agrupa outros blocos;
	  - Section       : 1 a 3 textos com um "acessório" ao lado (imagem ou botão);
	  - Thumbnail     : imagem pequena, usada como acessório de uma section;
	  - Media Gallery : galeria com 1 a 10 imagens;
	  - Action Row    : linha de botões ou menu, como nas mensagens comuns.

	Basta adicionar um desses blocos ao message builder: o plugin marca a
	mensagem como Components V2 sozinho. Nesse formato não há embeds, e o
	texto do builder vira o primeiro Text Display.
*/

#include <open.mp>
#include <discord-bridge>

#define CANAL_PAINEL "123456789012345678"
#define IMAGEM "https://assets.open.mp/assets/images/assets/logo-light-trans.png"

main()
{
}

public DBR_OnReady()
{
	new DiscordChannel:canal = DBR_FindChannelByID(CANAL_PAINEL);
	EnviarPainel(canal);
	EnviarAvisoSimples(canal);
	return 1;
}

EnviarPainel(DiscordChannel:canal)
{
	// Caixa com barra lateral azul.
	new DiscordComponent:caixa = DBR_CreateContainer(0x5865F2);

	DBR_AddComponent(caixa, DBR_CreateTextDisplay("# Meu Servidor RP\nBem-vindo! Tudo o que você precisa está aqui."));
	DBR_AddComponent(caixa, DBR_CreateSeparator());

	// Section com imagem ao lado.
	new DiscordComponent:sobre = DBR_CreateSection();
	DBR_AddComponent(sobre, DBR_CreateTextDisplay("## Sobre"));
	DBR_AddComponent(sobre, DBR_CreateTextDisplay("Economia realista, empregos e eventos semanais."));
	DBR_SetSectionAccessory(sobre, DBR_CreateThumbnail(IMAGEM, "Logo do servidor"));
	DBR_AddComponent(caixa, sobre);

	// Section com botão ao lado.
	new DiscordComponent:regras = DBR_CreateSection();
	DBR_AddComponent(regras, DBR_CreateTextDisplay("**Regras**\nLeia antes de entrar no jogo."));
	DBR_SetSectionAccessory(regras, DBR_CreateButton(DISCORD_BUTTON_SECONDARY, "Ler regras", "painel:regras"));
	DBR_AddComponent(caixa, regras);

	DBR_AddComponent(caixa, DBR_CreateSeparator(true, DISCORD_SPACING_LARGE));

	// Galeria de imagens.
	new DiscordComponent:galeria = DBR_CreateMediaGallery();
	DBR_AddMediaGalleryItem(galeria, IMAGEM, "Cidade");
	DBR_AddMediaGalleryItem(galeria, IMAGEM, "Praia");
	DBR_AddComponent(caixa, galeria);

	// Linha de botões dentro da caixa.
	new DiscordComponent:botoes = DBR_CreateActionRow();
	DBR_AddComponent(botoes, DBR_CreateButton(DISCORD_BUTTON_SUCCESS, "Quero jogar", "painel:jogar"));
	DBR_AddComponent(botoes, DBR_CreateButton(DISCORD_BUTTON_LINK, "Site", "https://open.mp"));
	DBR_AddComponent(caixa, botoes);

	new DiscordMessageBuilder:mensagem = DBR_CreateMessageBuilder();
	DBR_AddBuilderComponent(mensagem, caixa);
	DBR_SendMessage(canal, mensagem);
}

EnviarAvisoSimples(DiscordChannel:canal)
{
	// Components V2 sem caixa: o texto do builder vira o primeiro bloco.
	new DiscordMessageBuilder:mensagem = DBR_CreateMessageBuilder("### Manutenção hoje às 22h");
	DBR_AddBuilderComponent(mensagem, DBR_CreateSeparator());
	DBR_AddBuilderComponent(mensagem, DBR_CreateTextDisplay("O servidor ficará fora do ar por cerca de 30 minutos."));
	DBR_SendMessage(canal, mensagem);
}

public DBR_OnButton(DiscordInteraction:interaction, DiscordUser:user, const custom_id[])
{
	#pragma unused user
	if (!strcmp(custom_id, "painel:regras"))
	{
		return DBR_RespondInteraction(interaction, "1. Respeite todos.\n2. Nada de trapaças.\n3. Divirta-se!", .ephemeral = true);
	}
	if (!strcmp(custom_id, "painel:jogar"))
	{
		return DBR_RespondInteraction(interaction, "Conecte em `meuservidor.com:7777`.", .ephemeral = true);
	}
	return 1;
}
