# AGENTS.md — leia antes de mexer no RDoom NX

Guia para qualquer IA (ou pessoa) que assumir este projeto. Ele resume o que é
o projeto, as regras do dono e onde está o resto do conhecimento. O projeto foi
desenvolvido de 2026-09-28 a 2026-10-20 quase inteiramente com o Claude Code
(Claude Opus 5.5) e foi declarado **concluído** pelo dono na versão **1.1.4**.

## O que é

**RDoom NX**: port não oficial do **UZDoom 5.0.3** para Nintendo Switch
desbloqueado (homebrew `.nro`, Atmosphère), renderizado **só em Vulkan**
(Mesa NVK + compilador NAK ligados estaticamente), com um launcher próprio
(perfis, bibliotecas de IWADs e mods, ordem de carga). Um único executável:
`RDoom.nro`. Alvo: **Switch Lite, portátil, clocks de fábrica** (sem dock,
sem overclock).

- Repositório principal: https://github.com/aeon1911/RDoom-NX (branch `main`)
- Motor (submódulo `engine/`): https://github.com/aeon1911/RDoom-NX-engine
  (fork do UZDoom, branch `rdoom-nx`; remote local `rdoom`)
- Autor: **Renan (Æon)** (GitHub `aeon1911`)

## Ordem de leitura

1. Este arquivo (regras e mapa geral).
2. `docs/ai/01-HISTORIA.md` — linha do tempo do início ao fim, versão a versão.
3. `docs/ai/02-PROBLEMAS-E-SOLUCOES.md` — cada problema enfrentado: sintoma,
   causa real, como foi descoberta, correção e lição.
4. `docs/ai/03-MAPA-DO-CODIGO.md` — onde fica cada sistema e as CVARs do port.
5. `docs/ai/04-FLUXOS-DE-TRABALHO.md` — build, testes, Ryujinx, análise de logs
   do console, release.
6. `docs/ai/05-LIMITES-E-ARMADILHAS.md` — o que não tem solução, becos sem
   saída já explorados, armadilhas do emulador.

Documentos para usuários (pt-BR): `README.md`, `docs/STATUS.md` (histórico
detalhado de cada teste no console), `docs/VULKAN.md`, `docs/ARCHITECTURE.md`,
`docs/PERFORMANCE.md`, `docs/PROFILES.md`, `docs/MOD_COMPATIBILITY.md`,
`docs/RYUJINX_TEST_REPORT.md`, `docs/PORTING_AUDIT.md`.

## Regras do dono (obrigatórias)

Comunicação
- **Responder sempre em português do Brasil**, inclusive notas curtas de
  progresso. Código, comentários e mensagens de commit podem ser em inglês.
- Ao concluir qualquer item, terminar com a lista do **que ainda falta**.
- O que importa é o desempenho **dentro dos mapas**; hubs e menus lentos (ex.:
  hub do Lexicon a ~33 fps) não são prioridade.

Técnicas
- **Só Vulkan.** OpenGL/GLES foram removidos (`HAVE_OPENGL=OFF`,
  `HAVE_GLES2=OFF`). Não reintroduzir nem propor fallback para GL. Uma falha de
  Vulkan é fatal, com mensagem; nunca transformá-la em fallback silencioso.
- **Sem overclock** de nenhum tipo (verificado por `tests/check_no_overclock.sh`).
- **Não aumentar o ticrate** da simulação (35 Hz, igual ao UZDoom).
- **Não copiar código do GZDoom-NX-Modern** (Thorhax). Estudar para entender
  um problema é permitido; a solução tem de ser própria e não pode custar fps.
- Não resolver problemas desabilitando mods nem removendo arquivos do usuário.
- Arte (ícone, fundos) **a partir dos gráficos do Freedoom 2**, nunca arte
  vetorial/procedural (o dono rejeitou isso com veemência).

Arquivos e dados
- Nunca distribuir IWADs comerciais nem mods. Nunca commitar `*.wad`, `*.pk3`,
  `test-env/`, `evidence/`, `build/`, `dist/`, `third_party/mesa-sdk`,
  `third_party/freedoom` (o `.gitignore` cobre). Zips de release podem conter
  só Freedoom e os pk3 do próprio UZDoom/RDoom.
- `DOOM2.wad` na raiz é a IWAD comercial do dono: só para testes locais.
- Não modificar as pastas originais do dono (leitura é permitida):
  `/mnt/steam/UZDOOM SWITCH PORT`, `/mnt/steam/Projetos/UZDOOM-nro`,
  `/mnt/steam/DOOM MODs DataBase`, `~/.config/Ryujinx`.
- Não executar `git reset --hard`, não descartar modificações existentes, não
  sobrescrever arquivos do usuário.
- Não montar/desmontar discos.

Git e publicação
- Autor dos commits: `RDoom <171243930+aeon1911@users.noreply.github.com>`.
  Nunca commitar com o e-mail pessoal do dono.
- O e-mail `renanol.design@gmail.com` só pode aparecer publicamente como
  endereço de doação PIX/PayPal (README, notas de release, tela "Sobre").
- Texto de custo é do dono e não deve ser reescrito: "CRÉDITOS GASTOS DO MEU
  BOLSO COM API ≈ US$ 261".
- Nunca dar push na branch do motor `rdoom-nx-backup-before-email` (tem o
  e-mail antigo), se ela ainda existir.
- O dono edita o README direto no GitHub: **fazer fetch e merge de
  `origin/main` antes de cada push**.
- Push com o helper do gh:
  `git -c credential.helper= -c credential.helper='!gh auth git-credential' push ...`
  A rede do dono é instável (erros TLS "unexpected eof"): repetir o comando.

Testes
- Teste em Switch físico é sempre do dono. Resultado de emulador não é
  resultado de console; não declarar algo "testado no Switch" sem logs dele.
- **Nunca rodar o Ryujinx enquanto o SD do Switch estiver montado por MTP
  (DBI)**: travou o PC do dono. Conferir com `mount | grep -ci mtp` (precisa dar 0).
- Capturas de tela só da janela do Ryujinx (`scripts/vpad.py`, comando `shot`);
  nunca da área de trabalho.
- Logs do console: copiar primeiro para `evidence/hw/<data-nome>/` (o dono
  apaga do SD logo depois de enviar). Nos últimos envios eles vinham em
  `/mnt/steam/Projetos/RDoom-Ollama/logs/`. Olhar o conteúdo antes de copiar:
  a pasta antiga `/mnt/bf417055-…` passou a ter arquivos pessoais sem relação.

## Estado final (1.1.4, 2026-10-20)

- Motor `5.0.3-66-g1eb49010f`; release https://github.com/aeon1911/RDoom-NX/releases/tag/v1.1.4
- Estável no console com Freedoom/DOOM II, Brutal Doom, Project Brutality,
  Lexicon (+ slaughter), Compendium, Ashes 2063, Aliens Eradication TC,
  Trailblazer e WadFusion (gerado com WadFusion v1.6.1). 55–60 fps na maior
  parte; ver tabela em `README.md`.
- Limite que fica: lutas com muito gore/explosões no Project Brutality
  (36–50 fps) e mapas slaughter gigantes com Brutal Doom são limitados pela
  **CPU** do Switch (scripts do mod), não pela GPU. Detalhes em
  `docs/ai/05-LIMITES-E-ARMADILHAS.md`.

## Atalhos

```
scripts/build.sh            # build completo em Docker + pacote + testes de host
scripts/dev-rebuild.sh      # incremental (ninja dentro do Docker) + pacote
scripts/test.sh             # testes de host (precisa de ninja instalado no host)
scripts/ryujinx-test.sh <nome> <roteiro vpad|-> [segundos]
```
