# Arquitetura do RDoom NX

Um único executável, `RDoom.nro`, contém o launcher "RDoom NX" e o motor
UZDoom 5.0.3. O launcher roda primeiro, no mesmo processo, desenhando num
framebuffer de software; ao confirmar um perfil ele libera a tela e chama o
`main` do motor com a linha de comando montada a partir do perfil.

```
hbloader / Atmosphère
   └─ RDoom.nro
       ├─ userAppInit ............ nx_tls_bridge: TPIDR_EL0 = ponteiro de thread (NAK/Rust)
       ├─ I_NXRunWithBigStack .... thread principal com pilha grande
       │    ├─ RDoom_NXEarlyInit . chdir sdmc:/switch/rdoom, cria pastas, abre logs
       │    ├─ NX_NvCompatInit ... sonda de ALLOC_SPACE fixo (só age no emulador)
       │    ├─ RDoom_NXRunLauncher  launcher (UI, perfis, bibliotecas) → LaunchPlan
       │    └─ main(argc, argv) .. UZDoom: tela de carregamento, IWAD/mods, jogo
       └─ RDoom_NXAtExit ......... em hardware, volta para o launcher (envSetNextLoad)
```

## Componentes

### `engine/` — UZDoom 5.0.3 + patches (git, branch `rdoom-nx`)

Base: tag 5.0.3 (`7910df7102033fd7754a317f3254f24101135926`). Por cima, o port
de Switch anterior do usuário (commits `cfe330446`…`a5b622b9c`), a importação
do trabalho de Vulkan em andamento (`fd9999c71`) e os commits do RDoom
(`d65b6d187` em diante). A série completa está exportada em `patches/`.
Código específico do Switch:

| Arquivo | Papel |
|---|---|
| `src/common/platform/posix/nx/i_nxsystem.cpp` | entrada, pilha grande, crash handler (`logs/crash.log`), migalhas de boot |
| `.../nx/rdoom_nx.cpp` | layout do SD, logs, plataforma do launcher (framebuffer, controles, teclado), relatório de sessão `logs/engine-session.txt` |
| `.../nx/nx_tls_bridge.c` | `TPIDR_EL0` por thread (`--wrap=threadCreate`) |
| `.../nx/nx_nvcompat.c` | compatibilidade de reserva de VA no Ryujinx |
| `.../nx/nx_loadscreen.cpp` | tela de carregamento "RDOOM" |
| `.../nx/nx_glthread.cpp` | migalhas e relatório de travamento das threads do Mesa |
| `src/common/engine/nx_perf.cpp` | `perf.log` (tempos por frame e resumos a cada 10 s) |
| `src/common/rendering/vulkan/textures/vk_nxupload.cpp` | upload de texturas por imagem linear + blit |
| `src/common/platform/posix/sdl/sdlglvideo.cpp` | criação do Vulkan; se falhar, o jogo para com a mensagem do erro |

### `launcher/` — RDoom NX (C++17, sem dependências do motor)

| Módulo | Papel |
|---|---|
| `core/paths` | layout do SD |
| `core/fsutil` | escrita atômica (tmp + .bak + rename) |
| `core/archive` | leitura de WAD, ZIP/PK3 (inclui ZIP64) e 7z/PK7 |
| `core/content` | identificação de IWAD (iwadinfo do UZDoom), tipo de mod, GAMEINFO, TITLEPIC, incompatibilidades conhecidas |
| `core/profile` | perfis JSON versionados, modelos, migração e backup |
| `core/library` | varredura de `iwads/` e `mods/`, resolução dos arquivos de cada perfil |
| `core/launch` | CVars e argv do motor, `vid_preferbackend` no ini do perfil |
| `ui/*` | telas (Início, Perfis, Editor, IWADs, Mods, Ordem de carga, Configurações, Diagnóstico), fontes (stb_truetype), miniaturas |

O launcher também compila para Linux (testes de host e `tests/ui_preview`).

### Mesa (binário)

`third_party/mesa-sdk/…606441c9.zip`, verificado por SHA-256 no Dockerfile:
NVK (Vulkan) e o compilador NAK. O RDoom usa só o Vulkan; o backend OpenGL/GLES
do UZDoom não é compilado (`HAVE_OPENGL=OFF`, `HAVE_GLES2=OFF`). O EGL do SDK
só entra no link porque o SDL2 do devkitPro depende dele. Ver `docs/VULKAN.md`.

## Layout no cartão SD

```
sdmc:/switch/rdoom/
  RDoom.nro  uzdoom.pk3  game_support.pk3  game_widescreen_gfx.pk3
  brightmaps.pk3  lights.pk3  soundfonts/  fm_banks/  licenses/  VERSION.txt
  iwads/            IWADs do usuário (nunca apagadas)
  mods/             mods do usuário (subpastas permitidas)
  profiles/         <id>.json (+ .bak da versão anterior)
  config/           launcher.json; engine/<perfil>.ini (config do motor por perfil)
  saves/<perfil>/   saves por perfil
  screenshots/
  cache/            miniaturas, fundo/infos da tela de carregamento
  launcher/         arte do launcher
  logs/             rdoom.log, engine.log, engine-stderr.log, engine-session.txt,
                    boot.log, crash.log
  backup/ export/   cópias de segurança e perfis exportados
  perf.log          medições (nx_perflog)
```

## Laço do jogo e renderização

- Simulação a 35 Hz (inalterada). A renderização interpola entre tics; o
  ritmo dos frames vem do vsync (FIFO) e de `vid_maxfps` (padrão do perfil:
  vsync ligado, 60).
- Vulkan: um submit por frame; o fim do frame não espera a GPU — a espera fica
  para o início do frame seguinte (`rd_vk_frameoverlap`). Buffers persistentes
  ficam em memória coerente sem cache, sem limpeza de cache por submit.

## Logs e diagnóstico

| Arquivo | Conteúdo |
|---|---|
| `logs/rdoom.log` | launcher: varredura, perfis, plano de inicialização |
| `logs/engine-session.txt` | passos do renderizador (pedido, ativo, instância, dispositivo, swapchain, frames) |
| `logs/engine.log` / `engine-stderr.log` | console do motor |
| `logs/boot.log` | migalhas de inicialização (sem buffer) |
| `logs/crash.log` | endereço e pilha em caso de crash |
| `perf.log` | resumos de desempenho; `nx_perflog 2` registra todo frame |

## Build

`scripts/build.sh` (completo, em Docker `rdoom-nx-build`) e
`scripts/dev-rebuild.sh` (incremental). O pós-build do CMake gera o NACP,
remove símbolos, aplica `patch_chkfeat.py` e gera o NRO. `scripts/package.sh`
monta `dist/RDoom-NX/` e os zips com `SHA256SUMS`.
