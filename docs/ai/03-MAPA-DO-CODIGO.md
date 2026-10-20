# 03 — Mapa do código

Onde fica cada peça e cada sistema do port. Os caminhos de `src/` e `wadsrc/`
são relativos a `engine/`.

## Estrutura do repositório

| Pasta | Conteúdo |
|---|---|
| `engine/` | submódulo: UZDoom 5.0.3 (tag `7910df7102033fd7754a317f3254f24101135926`) + 66 commits do port, branch `rdoom-nx` |
| `patches/` | a mesma série exportada (`git -C engine format-patch 7910df71..HEAD`) + `SHA256SUMS.txt` |
| `launcher/` | launcher RDoom NX em C++17, sem dependência do motor (compila também no Linux para testes) |
| `docker/Dockerfile` | imagem `rdoom-nx-build`: devkitA64 (GCC 15.2, libnx 4.12), portlibs, libsndfile com Xiph, Mesa SDK verificado por SHA-256 |
| `scripts/` | build, pacote, testes de host e no Ryujinx, benchmarks, ferramentas |
| `tests/` | testes de host do launcher, smoke de UI, verificação de pacote e de ausência de overclock |
| `assets/` | `src/generate_art.py` (arte a partir do `freedoom2.wad`), `rdoom-pipelines-seed.bin`, `prompts/` |
| `vktest/` | NROs de teste de Vulkan (`vktest.nro` fumaça, `matrix/` matriz de recursos) |
| `docs/` | documentação (pt-BR); `docs/ai/` é este conjunto |
| `.github/workflows/build.yml` | CI: `scripts/build.sh` no GitHub Actions |
| locais, fora do git | `build/`, `dist/`, `evidence/` (logs do console e do emulador), `test-env/` (Ryujinx isolado, conteúdo de teste), `third_party/` (Mesa SDK, Freedoom) |

## Fluxo do executável

```
hbloader / Atmosphère
 └─ RDoom.nro
     ├─ userAppInit ........... nx_tls_bridge: TPIDR_EL0 (NAK/Rust)
     ├─ I_NXRunWithBigStack ... thread principal (núcleo 0, prioridade 0x2C)
     │    ├─ RDoom_NXEarlyInit . chdir sdmc:/switch/rdoom, pastas, logs
     │    ├─ NX_NvCompatInit ... sonda do ALLOC_SPACE (só age no emulador)
     │    ├─ RDoom_NXRunLauncher  launcher → LaunchPlan (argv do motor)
     │    └─ main(argc, argv) .. UZDoom
     └─ RDoom_NXAtExit ........ no hardware, volta ao launcher (envSetNextLoad)
```

## Código específico do Switch no motor

| Arquivo | Papel |
|---|---|
| `src/common/platform/posix/nx/i_nxsystem.cpp` | entrada, pilha grande, crash handler (`logs/crash.log`), afinidade e prioridade de threads (`I_NXSetupWorkerThread`) |
| `.../nx/rdoom_nx.cpp` | layout do SD, logs, plataforma do launcher, relatório de sessão (`RDoom_SessionSet`), escrita em memória (`RDoom_BufferFile`), thread de gravação |
| `.../nx/nx_tls_bridge.c` | `TPIDR_EL0` por thread (`--wrap=threadCreate`) |
| `.../nx/nx_nvcompat.c` | compatibilidade de reserva de VA no Ryujinx |
| `.../nx/nx_loadscreen.cpp` | tela de carregamento "RDOOM" (thread própria) |
| `.../nx/nx_glthread.cpp` | migalhas e relatório de travamento das threads do Mesa |
| `.../nx/nx_vktrace.cpp` | rastreamento de `vkCmd*` (build com `-DRDOOM_VK_TRACE=ON`) |
| `.../nx/i_specialpaths.cpp` | caminhos de config, saves e cache no SD |
| `src/common/platform/posix/sdl/sdlglvideo.cpp` | criação do Vulkan; falha é fatal com mensagem |
| `src/common/engine/nx_perf.cpp/.h` | `perf.log`: linhas por frame lento, resumos a cada 5 s, notas |
| `src/common/engine/nx_jobs.cpp` | workers em segundo plano (`nx_workers 2`, núcleos 1 e 2, prioridade 0x3B) |
| `src/common/engine/nx_governor.cpp` | estabilizador (`nx_stabilizer`): corta fumaça, depois luzes |
| `src/common/objects/nx_objpool.cpp` | alocador de objetos de script e de mallocs ≤ 1 KB |
| `src/rendering/hwrenderer/hw_nxprefetch.cpp` | pré-carga das imagens de classes de ator que aparecem no nível |
| `src/rendering/hwrenderer/hw_nxlights.cpp` | orçamento de luzes dinâmicas (`nx_dynlight_max`, `nx_dynlight_dist`) |
| `src/rendering/hwrenderer/scene/hw_nxeffect.h` | `NXIsPureEffect` (o que conta como efeito: fumaça, fogo, projéteis sem dano) |
| `src/rendering/hwrenderer/scene/hw_sprites.cpp` | `NXEffectOverBudget` (orçamento de área), `rd_fx_nearcull`, envio à camada de meia resolução |
| `src/rendering/rdoom_gpubench.cpp` | benchmark de GPU no console (`rd_gpubench=1` → `logs/gpu-bench.txt`) |
| `src/common/filesystem/source/files.cpp` | leitura de arquivos sem segurar mutex durante I/O do SD |
| `src/common/audio/sound/s_sound.cpp` | carga assíncrona de sons, reinício de canais pendentes (`NXRestartPending`), contadores do `perf.log` |
| `src/common/utility/m_alloc.cpp` | erro de memória com quem pediu e memória em uso |
| `src/common/rendering/hwrenderer/hw_noopengl.cpp` | símbolos que o motor espera dos arquivos GL (build sem OpenGL) |
| `libraries/ZMusic/source/decoder/mpg123_decoder.cpp` | `MPG123_QUIET` no Switch |

Os arquivos `src/common/rendering/gles/*` (do port antigo) continuam no
repositório, mas **não são compilados** (`HAVE_GLES2=OFF`).

## Backend Vulkan (`src/common/rendering/vulkan/`)

| Arquivo | O que tem do port |
|---|---|
| `system/vk_renderdevice.cpp` | `BeginFrame`; `UpdateDynamicResolution`; `EvictStaleTextures` (fila de liberação de texturas); `NXWarmHalfEffects`; destrutor que esvazia frames pendentes; contador `RDoom_VkFrameNumber` |
| `system/vk_commandbuffer.cpp` | dois frames em voo, bancos de fence, `WaitForPendingFrame`, envio antecipado da cena, listas de exclusão por frame |
| `system/vk_hwbuffer.cpp` | buffers escritos direto na memória unificada; limpeza de cache só em memória cacheada |
| `textures/vk_nxupload.cpp` | upload de textura por imagem linear + `vkCmdBlitImage` |
| `textures/vk_asynctex.cpp` | decodificação de sprites nos workers; a thread do jogo só copia para a GPU (`rd_vk_async_upload_ms`) |
| `textures/vk_hwtexture.cpp/.h` | `VkMaterial::LastUseFrame` (marcado em `GetDescriptorSet`), registro na lista de pré-carregamento |
| `textures/vk_renderbuffers.cpp` | cena RGBA8, profundidade sem ZCULL, alvos da camada de meia resolução |
| `renderer/vk_renderpass.cpp` | cache e lista de pipelines; lista de texturas (`TextureFirstUsed`, `LoadTextureList`, `PrecacheListedTextures`) |
| `renderer/vk_renderstate.cpp` | seleção do shader `EFF_SIMPLE2D`; desenho na camada de meia resolução (`mNXFxLayer`) |
| `renderer/vk_descriptorset.cpp/.h` | `GetMaterials()`; imagens compartilhadas (`IsSharedPicture`) para a liberação de texturas |

Shaders do port em `wadsrc/static/shaders/`:
- `glsl/main.fp`: `getLightRange()` (validação do cabeçalho de luzes, P09) e o
  caminho `SIMPLE_2D`;
- `pp/upscale.fp`: ampliação da cena;
- `pp/nxdepth.fp`: profundidade da camada de meia resolução.

O `hw_shaderpatcher.cpp` registra o efeito `EFF_SIMPLE2D` (`hw_renderstate.h`).

## Sistemas de desempenho (resumo de como funcionam)

1. **Sobreposição de frames:** fim do frame sem esperar a GPU; espera no início
   do próximo. Dois bancos de buffers por frame.
2. **Envio antecipado:** a cena 3D vai para a GPU ao terminar e em blocos de 300
   desenhos; só o pós e o 2D esperam a imagem da swapchain.
3. **Resolução dinâmica:** só a cena 3D encolhe (60–100%), guiada pelos
   timestamps da GPU e por médias sustentadas de quadro/espera; arma e HUD em
   resolução cheia.
4. **Camada de meia resolução:** fumaça e fogo grandes desenhados em 640×360 e
   misturados sobre a cena; desliga sozinha se a leitura de profundidade falhar.
5. **Estabilizador e orçamento de área:** corta efeitos translúcidos de perto
   para longe quando o quadro atrasa; luzes só em níveis altos.
6. **Pipelines e texturas no carregamento:** lista de pipelines (semente no
   pacote + lista do cartão) e lista de texturas usadas (recentes primeiro,
   até 2000, orçamento de 256 MB; acima de 1,8 GB de heap só as imagens grandes
   da HUD, até 64 MB).
7. **Sprites em segundo plano:** decodificação nos workers; um sprite novo fica
   de fora 1–2 frames (o ator mantém o quadro anterior).
8. **Liberação de texturas:** acima de 2300 MB de heap, sprites, decalques e HUD
   sem uso há 30 s saem 4 por quadro; imagens compartilhadas e HUD grandes ficam.
9. **Alocador de objetos:** objetos de script e mallocs pequenos sem a newlib.
10. **SD fora do jogo:** logs em memória, thread de gravação no núcleo 2,
    listas gravadas raramente e por worker.
11. **Shader 2D enxuto** para HUDs e menus.

## CVARs do port

Padrões no Switch. Ficam no `.ini` do perfil, então para mudar um padrão de
CVAR arquivada use a migração `rd_defaults_rev`.

| CVar | Padrão | Função |
|---|---|---|
| `rd_vk_frameoverlap` | 1 | sobreposição CPU/GPU entre frames |
| `rd_vk_earlysubmit` | 1 | envio antecipado da cena (`vk_submit_size 300`) |
| `rd_vk_blitupload` | 1 | upload por imagem linear + blit |
| `rd_vk_scenefmt` | 2 | formato da cena (0 RGBA16F, 1 RGB10A2, 2 RGBA8) |
| `rd_vk_sceneswap` | 1 | sem cópia da cena para o pós |
| `rd_vk_nofans` | 1 | triangle fans como triângulos indexados |
| `rd_vk_depthbias` | 0 | depth bias real (0 = emulado pelo viewport, `rd_vk_depthbias_eps`) |
| `rd_vk_zcull` | 0 | ZCULL nas profundidades (desligado) |
| `rd_vk_depth32`, `rd_vk_zstate`, `rd_vk_drawlog` | 0 | diagnóstico do "device lost" |
| `rd_vk_gputime` | 1 | timestamps de GPU por frame |
| `rd_vk_halfeffects` | 1 | camada de meia resolução (`rd_halfeffects_minscreen 0.6`, `rd_halfeffects_minarea 0.004`, `rd_halfeffects_debug`) |
| `rd_vk_simple2d` | 1 | shader 2D enxuto |
| `rd_vk_async_textures` | 1 | sprites decodificados em segundo plano (`rd_vk_async_upload_ms 3`) |
| `rd_async_sprites`, `rd_sprite_prefetch` | 1 | pré-carga e decodificação assíncrona de sprites |
| `rd_vk_texwarm_mb` | 256 | orçamento do pré-carregamento de texturas |
| `rd_vk_texevict_mb` | 2300 | heap (MB) acima do qual texturas sem uso são liberadas (0 desliga) |
| `rd_vk_texevict_perframe` | 4 | imagens liberadas por quadro |
| `rd_dynres` | 1 | resolução dinâmica (`rd_dynres_min 0.6`, `rd_dynres_target 12`, `rd_dynres_force` para testes) |
| `rd_sprite_alphacut` | 0.02 | descarte de fragmentos quase transparentes |
| `rd_fx_nearcull` / `rd_fx_nearfade` | 20 / 48 | efeitos colados na câmera somem/esmaecem |
| `rd_gpubench` | 0 | benchmark de GPU no console |
| `rd_joy_sensitivity` / `rd_joy_deadzone` / `rd_joy_invert_look` | -1 (usa o perfil) | controles por perfil |
| `rd_defaults_rev` | — | revisão da migração de padrões |
| `nx_stabilizer` | 60 | estabilizador (alvo em fps; 0 desliga) |
| `nx_effect_minalpha` | 0.04 | efeitos abaixo desse alfa não são desenhados |
| `nx_effect_area`, `nx_effect_cull` | 0 | ajustes do orçamento de efeitos |
| `nx_dynlight_max`, `nx_dynlight_dist` | 0 | limites de luzes dinâmicas (0 = do estabilizador) |
| `nx_workers` | 2 | workers em segundo plano |
| `nx_perflog` | 1 | `perf.log` (2 = todo frame) |
| `nx_perfthreshold` | 40 | ms a partir do qual um frame vira linha no `perf.log` |
| `r_ticstability` | 0 no Switch | espera ocupada do UZDoom, desligada |
| `vid_preferbackend` | 1 | forçado para Vulkan |

Variáveis do driver Mesa e CVARs podem ser ligadas sem novo build pelo
`sdmc:/switch/rdoom/rdoom_debug.txt` (modelo comentado em
`rdoom_debug.exemplo.txt`; MAIÚSCULAS = variáveis do driver, como
`NVK_DEBUG=push_sync`; minúsculas = CVARs).

## Launcher (`launcher/src/`)

| Módulo | Papel |
|---|---|
| `core/paths` | layout do SD |
| `core/fsutil` | escrita atômica (tmp + .bak + rename) |
| `core/archive` | leitura de WAD, ZIP/PK3 (com ZIP64) e 7z/PK7 |
| `core/content` | identificação de IWAD (iwadinfo do UZDoom), tipo de mod, GAMEINFO, TITLEPIC, incompatibilidades e conflitos de CVARINFO |
| `core/profile` | perfis JSON versionados, modelos (Freedoom, Brutal Doom, Project Brutality, WadFusion, Lexicon, Compendium, personalizado), migração |
| `core/library` | varredura de `iwads/` e `mods/` |
| `core/launch` | argv do motor e CVars do perfil |
| `ui/*` | telas, fontes (stb_truetype), miniaturas, cartões de perfil |

Formato e regras dos perfis em `docs/PROFILES.md`.

## Layout no SD

```
sdmc:/switch/rdoom/
  RDoom.nro uzdoom.pk3 game_support.pk3 game_widescreen_gfx.pk3 brightmaps.pk3
  lights.pk3 rdoom-pipelines-seed.bin soundfonts/ fm_banks/ licenses/ VERSION.txt
  iwads/ mods/              conteúdo do usuário (nunca apagado)
  profiles/<id>.json        perfis (+ .bak)
  config/launcher.json      config/engine/<perfil>.ini
  saves/<perfil>/  screenshots/  backup/  export/
  cache/                    miniaturas; cache/uzdoom/ listas de pipelines e texturas
  logs/                     rdoom.log engine.log engine-stderr.log engine-session.txt
                            boot.log phase.log perf.log crash.log gpu-*.txt
                            anterior/ … anterior-5/ (últimas cinco sessões)
```

Atualizar copiando a pasta inteira apaga `cache/`. Por isso o pacote traz a
semente de pipelines.
