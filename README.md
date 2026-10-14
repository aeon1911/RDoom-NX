# RDoom NX

Port não oficial do **UZDoom 5.0.3** para Nintendo Switch desbloqueado
(Atmosphère, homebrew `.nro`), renderizado em **Vulkan** (Mesa NVK), com um
launcher próprio, o **RDoom NX**: bibliotecas de IWADs e mods, ordem de
carregamento e perfis.

Feito por **Renan (Æon)**.

**Atualmente a melhor experiência jogável de DOOM w/MODS do Nintendo Switch!**

**Somente os clocks de fábrica.** O RDoom não tem código de overclock, nem
opção de frequência, nem dependência de sys-clk (verificado por
`tests/check_no_overclock.sh`).

## Apoie o projeto

O RDoom é gratuito e de código aberto. Se ele te divertiu e quiser ajudar:

| | |
|---|---|
| **PIX** | `renanol.design@gmail.com` |
| **PayPal** | `renanol.design@gmail.com` |

## Quanto custou

O RDoom foi desenvolvido com o **Claude Code** (modelo Claude Opus 5.5, da
Anthropic), em sessões com testes no Switch Lite do autor a cada etapa.

| | |
|---|---|
| CRÉDITOS GASTOS DO MEU BOLSO COM API | **≈ US$ 261** |

O valor soma os tokens registrados nas sessões do projeto, com a
tabela pública do Claude Opus 5.5 (outubro de 2026): 1,93 milhão de tokens de
saída (US$ 38,65), 6,26 milhões de escrita de cache (US$ 50,08), 860 milhões de
leitura de cache (US$ 172,08) e 3,7 mil de entrada (US$ 0,01).

## Estado (1.1.0, 2026-10-14) — projeto concluído

Testado no Switch físico (Lite, portátil, clocks de fábrica):

| Conteúdo | Resultado |
|---|---|
| Freedoom 2, DOOM II | 58–60 fps (55–60 no 1.0.0; resolução dinâmica ajustada depois) |
| Brutal Doom v21 (Freedoom 2 / DOOM II) | 55–60 fps; fumaça e fogo em meia resolução nas lutas |
| Lexicon (+ pacote slaughter) | 59–60 fps em mapas médios; 45–60 com 1500–2000 monstros |
| Brutal Doom + Lexicon slaughter | 43–54 fps (limite: scripts do Brutal Doom na CPU, ~4000 objetos) |
| Trailblazer + Lexicon slaughter | 55–60 fps |
| Compendium, Ashes 2063, Aliens Eradication TC | 54–60 fps |
| Project Brutality (versão do GitHub) sobre DOOM II | 55–60 fps em combate; 58–60 sem o visor do capacete |
| WadFusion (gerado com o WadFusion v1.6.1) + Project Brutality | 57–60 fps (Sigil E5M1) |
| Brutal Doom + mapa slaughter gigante (7500 objetos) | 12–14 fps: limite da CPU, sem solução no port |

O que mantém o ritmo: resolução dinâmica guiada pelo tempo da GPU, fumaça e
fogo do Brutal Doom numa camada de meia resolução, estabilizador que corta
primeiro os efeitos mais caros, sprites decodificados em segundo plano,
pipelines e texturas preparados no carregamento da fase, alocador próprio para
os objetos do jogo e gravação no cartão fora da thread do jogo. Histórico de
testes em `docs/STATUS.md`.

## Instalação

1. Copie `switch/` do zip `RDoom-NX-1.1.0.zip` (ou `…-Freedoom.zip`, que já
   traz as IWADs livres do Freedoom) para a raiz do cartão SD. Para atualizar,
   copie por cima: perfis, saves e o cache de `switch/rdoom/cache` continuam.
2. IWADs em `sdmc:/switch/rdoom/iwads/`, mods em `sdmc:/switch/rdoom/mods/`.
3. Abra o RDoom pelo Homebrew Menu **em modo título** (segure R ao abrir um
   jogo). Escolha o perfil com L/R e aperte **+**.

Detalhes em `docs/INSTALL-pt-BR.txt` (também dentro do pacote).

## Perfis prontos

FreeDoom 2 — Vulkan Stock · Brutal Doom · Project Brutality · WadFusion · Lexicon · Compendium ·
Configuração personalizada, todos com a configuração **"RDoom — Stock
Switch"** (1280×720, vsync, 60 FPS, sem pós-processamento pesado). Criar,
duplicar, renomear, editar, apagar, exportar, importar e restaurar pelo
launcher. Antes de iniciar, o launcher avisa quando dois mods escolhidos
declaram as mesmas variáveis (CVARINFO) e por isso não rodam juntos. Formato e
regras em `docs/PROFILES.md`.

## Compilar

Requisitos no host: Docker, git, python3 (com Pillow), rsvg-convert, ImageMagick, curl,
unzip.

```bash
git clone --recursive https://github.com/aeon1911/RDoom-NX.git
cd RDoom-NX
scripts/build.sh
```

O motor é um submódulo ([RDoom-NX-engine](https://github.com/aeon1911/RDoom-NX-engine),
branch `rdoom-nx`). O build gera `dist/RDoom.nro`, `dist/RDoom-NX/` (árvore do
SD), os zips e `dist/SHA256SUMS.txt`. Incremental: `scripts/dev-rebuild.sh`.
Testes: `scripts/test.sh` (host) e `scripts/ryujinx-test.sh`,
`scripts/bench-ryujinx.sh`, `scripts/pacing-ryujinx.sh` (emulador).

## Estrutura

| Pasta | Conteúdo |
|---|---|
| `engine/` | UZDoom 5.0.3 + patches do port (submódulo) |
| `patches/` | a mesma série exportada (`git format-patch`) |
| `launcher/` | launcher RDoom NX (C++17) |
| `docker/` | imagem de build (devkitA64, libnx, Mesa SDK verificado) |
| `scripts/` | build, empacotamento, testes no emulador, benchmarks |
| `tests/` | testes de host, verificação de pacote e de overclock |
| `vktest/` | NROs de teste de Vulkan (fumaça e matriz de recursos) |
| `assets/` | arte do launcher (gerada por script a partir do Freedoom) e lista de pipelines do pacote |
| `docs/` | documentação |

## Documentação

- `docs/VULKAN.md` — como o Vulkan funciona aqui, o que foi corrigido, otimizações, critérios e provas
- `docs/PERFORMANCE.md` — benchmarks e ritmo de frames, método e ressalvas
- `docs/ARCHITECTURE.md` — fluxo de boot, módulos, layout do SD, logs
- `docs/PROFILES.md` — perfis e modelos
- `docs/MOD_COMPATIBILITY.md` — mods testados (e o WadFusion, que precisa ser gerado com a v1.6.1)
- `docs/RYUJINX_TEST_REPORT.md` — ambiente e resultados no emulador
- `docs/PORTING_AUDIT.md` — origem de cada parte, mudanças, política de clocks
- `docs/STATUS.md` — histórico de testes no console e pendências
- `THIRD_PARTY_NOTICES.md` — licenças

## Licença

GPL-3.0 (a do UZDoom). Componentes de terceiros em `THIRD_PARTY_NOTICES.md`.
"Doom" é marca dos seus donos; o RDoom não inclui dados comerciais (IWADs
comerciais e mods não são distribuídos: use os seus próprios arquivos ou o
Freedoom).

---

## English

**RDoom NX** is an unofficial port of **UZDoom 5.0.3** to the hacked Nintendo
Switch (Atmosphère homebrew `.nro`), rendered with **Vulkan** (Mesa NVK), with
its own launcher for IWAD/mod libraries, load order and profiles. Stock clocks
only: no overclocking code of any kind. Made by **Renan (Æon)**.

Tested on a Switch Lite (handheld, stock clocks): Freedoom 2 and DOOM II at
58–60 fps; Brutal Doom v21 at 55–60 fps; Lexicon with its slaughter pack (1500–2000
monsters) at 45–60 fps; Trailblazer, Compendium, Ashes 2063 and Aliens
Eradication TC at 54–60 fps; Project Brutality at 55–60 fps; WadFusion (built
with WadFusion v1.6.1) works. GPU-timed dynamic resolution, a lean 2D shader for
HUDs and menus, smoke and fire in a
half-resolution layer, background sprite decoding, pipelines and textures
prepared at level load, a pooled allocator for game objects and SD writes off
the game thread keep the frame rate up. The launcher warns before starting
when two selected mods declare the same CVARINFO variables (they cannot load
together).

**Install:** copy `switch/` from `RDoom-NX-1.1.0.zip` (or the `-Freedoom` zip,
which includes the free Freedoom IWADs) to the root of the SD card; IWADs go in
`sdmc:/switch/rdoom/iwads/`, mods in `sdmc:/switch/rdoom/mods/`. Start it from
the Homebrew Menu in title mode (hold R while opening a game). To update, copy
over the old files: profiles, saves and the pipeline cache are kept.

**Build:** `git clone --recursive https://github.com/aeon1911/RDoom-NX.git &&
cd RDoom-NX && scripts/build.sh` (Docker, git, python3 + Pillow, rsvg-convert,
ImageMagick, curl, unzip).

**What it cost:** developed with Claude Code (Anthropic's Claude Opus 5.5
model), with every step tested on the author's Switch Lite. The author paid
about **US$ 261** in API credits out of pocket (1.93 M output tokens, 6.26 M
cache writes, 860 M cache reads, 3.7 K input tokens; Claude Opus 5.5 list
prices, October 2026).

**Support the project:** PIX or PayPal to `renanol.design@gmail.com`.

GPL-3.0, like UZDoom. No commercial game data is included.
