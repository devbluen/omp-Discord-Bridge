# Exemplos do Discord Bridge

> English version: [../en](../en/README.md)

Cada arquivo é um gamemode pequeno e independente sobre um único assunto.
Leia na ordem se estiver começando: cada exemplo usa só o que os anteriores já
explicaram.

| Arquivo | Assunto |
| --- | --- |
| [01-primeiros-passos.pwn](01-primeiros-passos.pwn) | Conectar, enviar mensagens, callbacks, ler o chat (`!ping`) e tratar erros |
| [02-embeds.pwn](02-embeds.pwn) | Embeds completas, várias embeds em uma mensagem e painel que se atualiza |
| [03-slash-commands.pwn](03-slash-commands.pwn) | Comandos `/`, opções, escolhas, permissões, subcomandos, autocomplete e menus de contexto |
| [04-botoes.pwn](04-botoes.pwn) | Estilos de botão, cliques, handler por prefixo e atualizar a mensagem do botão |
| [05-menus-de-selecao.pwn](05-menus-de-selecao.pwn) | Menus de opções, usuários, cargos e canais |
| [06-modais.pwn](06-modais.pwn) | Formulários com campos de texto e menu, abertos por comando ou botão |
| [07-components-v2.pwn](07-components-v2.pwn) | Mensagens montadas com blocos: caixas, seções, galerias e separadores |
| [08-respostas-de-interacao.pwn](08-respostas-de-interacao.pwn) | Todas as formas de responder: privada, adiada, follow-up, editar e apagar |
| [09-moderacao.pwn](09-moderacao.pwn) | Expulsar, banir, desbanir, castigo, apelido, cargos e voz |
| [10-consultas.pwn](10-consultas.pwn) | Buscar servidor, membros, usuários, cargos e canais |
| [11-perfil-do-bot.pwn](11-perfil-do-bot.pwn) | Status, atividades, nome, avatar, banner e descrição do bot |
| [12-mensagens.pwn](12-mensagens.pwn) | Responder, reagir, editar, fixar, limpar canal e mandar DM |

## Como rodar um exemplo

1. Configure o token do bot como explicado no [README principal](../../README.pt-BR.md#3-configure-o-token).
2. Troque os IDs no topo do arquivo (`#define CANAL_...`) pelos IDs do seu servidor.
3. Compile o arquivo como gamemode e inicie o servidor.

Rode um exemplo por vez. Os callbacks `DBR_On...` são entregues ao primeiro
script que os tiver, então dois exemplos carregados juntos disputariam os
mesmos eventos.

> Comandos globais podem levar alguns minutos para aparecer no Discord. Para
> testar rápido, passe o servidor em `DBR_CreateCommand(..., DBR_FindGuildByID("ID"))`.
