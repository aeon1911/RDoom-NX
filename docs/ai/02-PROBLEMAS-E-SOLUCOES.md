# 02 — Problemas enfrentados e como foram resolvidos

Catálogo de todos os problemas relevantes, em ordem cronológica. Os números
(P01…) são citados em `01-HISTORIA.md`. Formato: **Sintoma**, **Causa**,
**Como foi achado**, **Correção** e **Lição** para o futuro. Os commits citados
são do motor (`engine/`, branch `rdoom-nx`), salvo indicação.

---

## A. Fazer o Vulkan (Mesa NVK) existir no Switch e no Ryujinx

### P01 — Linker não encontra símbolos do SDK do Mesa
- **Sintoma:** referências indefinidas ao ligar com `libvulkan.a` e as outras
  bibliotecas do SDK.
- **Causa:** os `.a` do SDK foram gerados com `llvm-ar` no MSYS2 e o índice de
  símbolos ficou incompleto para o `ld` do GNU.
- **Correção:** `docker/Dockerfile` roda `aarch64-none-elf-ranlib` em todas as
  bibliotecas do SDK.
- **Lição:** se o SDK do Mesa for trocado, conferir isso primeiro.

### P02 — Compilador de shaders (NAK, em Rust) trava ou cai
- **Causa:** o código Rust lê o ponteiro de thread em `TPIDR_EL0`, que o libnx
  nunca escreve.
- **Correção:** `nx_tls_bridge.c` copia `__aarch64_read_tp()` para `TPIDR_EL0`
  na thread principal e em toda thread criada (`-Wl,--wrap=threadCreate`;
  `pthread_create` também passa por ali).
- **Lição:** nenhum objeto do RDoom deve ler `TPIDR_EL0` diretamente; o build
  confere isso ("no RDoom object reads TPIDR_EL0").

### P03 — NVK não inicializa no Ryujinx
- **Causa:** o `nvhost-as ALLOC_SPACE` com endereço fixo do Ryujinx 1.3.3
  devolve o início do bloco livre, não o endereço pedido.
- **Correção:** `nx_nvcompat.c` detecta a peculiaridade com uma sonda e só
  então reserva a região de páginas grandes e responde as reservas fixas
  localmente. No console a sonda é normal (`nv_compat: inactive`).
- **Lição:** foi isso que também impedia o GZDoom-NX-Modern de passar de
  "Creating Vulkan instance" no mesmo emulador.

### P04 — Instruções que o Ryujinx não traduz
- **Causa:** o libgcc do GCC 15 traz CHKFEAT/GCS (`mrs gcspr_el0`, `gcspopm`).
- **Correção:** `scripts/patch_chkfeat.py` troca-as por equivalentes neutros no
  ELF (pós-build). No hardware, sem GCS, o comportamento é idêntico.

### P05 — Imagem preta no Vulkan (Ryujinx)
- **Causa:** o NVK executa `vkCmdCopyBuffer`/`FillBuffer`/`CopyBufferToImage`
  como shaders de compute internos (uma só família de filas), e o Ryujinx não
  os emula.
- **Correção:** memória unificada no Switch. Buffers são escritos direto na
  memória visível pela CPU (`vk_hwbuffer.cpp`), e as texturas sobem para uma
  imagem linear copiada com `vkCmdBlitImage`, que roda no motor 3D
  (`vk_nxupload.cpp`, `rd_vk_blitupload`). Isso também tira trabalho de compute
  da GPU real.
- **Pendência herdada:** lightmaps ainda usam `CopyBufferToImage`; nenhum mod
  testado usa lightmaps.

### P32 — Caminhos `sdmc:/` e `CompileNextShader`
- IWAD não encontrada porque `sdmc:/…` não era tratado como caminho absoluto;
  re-entrada em `CompileNextShader` com índice -1 ("Unable to load garbage").
  Ambos corrigidos no motor (fase 1).

## B. Primeiras otimizações (Ryujinx)

### P06 — 5,7 ms por frame limpando cache
- **Causa:** o port herdado fazia `armDCacheClean` em todos os buffers
  persistentes a cada submit. Os buffers `HOST_COHERENT` do NVK ficam em memória
  **sem cache** (tipo `1:LVCo`), então a limpeza era trabalho puro.
- **Como foi achado:** relatório `vulkan_memory_types` e desmontagem do SDK
  (`svcSetMemoryAttribute(..., Uncached)`).
- **Correção:** cada buffer/imagem sabe se a sua memória é `HOST_CACHED` e só
  nesse caso limpa. 97 → 177 fps (Freedoom MAP01, Ryujinx).

### P07 — CPU esperando a GPU no fim de cada frame
- **Correção:** `rd_vk_frameoverlap`. O fim do frame faz submit e present sem
  esperar; a espera fica no início do frame seguinte (`WaitForPendingFrame`),
  antes de qualquer buffer ser reescrito. Não é condição de corrida, como foi
  confirmado depois.

### P08 — `phase.log` gravado cinco vezes por frame no SD
- **Correção:** cada fase é registrada uma vez; o relatório de travamento usa
  uma "migalha" em memória. 201 → 367 fps.
- **Lição geral:** **qualquer gravação no SD durante o jogo causa engasgo no
  console.** Esse tema voltou várias vezes (P11, P16, P17, P19, P27).

## C. GPU caindo no console ("device lost")

### P09 — `device lost` / `channel 2 lost: fifo idle timeout`
- **Sintoma:** primeiro ao atirar, depois já ao entrar no mapa, em qualquer
  jogo. Menus 2D funcionavam. O Ryujinx não reproduzia.
- **Investigação (oito testes no console, 2026-10-11/12):**
  - Caches no SD (cache de pipelines gravado mesmo após o erro, cache de shaders
    do Mesa em `sdmc:/.mesa`): passaram a ser apagados automaticamente após uma
    perda (`logs/gpu-lost.txt`). **Não era a causa.**
  - Sobreposição de frames desligada: continuou caindo. **Não era.**
  - Mensagens do driver no log (`VK_EXT_debug_utils`) mostraram "fifo idle
    timeout", ou seja, a GPU travada, não falha de MMU.
  - Escada automática de contornos (`gpu_workaround`, `logs/gpu-workaround.txt`):
    `no_compression`, `NVK_SWITCH_NO_UBO_CBUF`, `MAPPED_COMPLETION=0`, D32S8. Nada
    resolveu.
  - Dump do push buffer (`NVK_DEBUG=push_sync`) apontou um desenho entre
    `LOAD_ZCULL`/`STORE_ZCULL`. O ZCULL foi desligado (profundidade
    `TRANSIENT_ATTACHMENT`), mas o console **ainda travava**, então não era isso.
  - Triangle fans viraram triângulos indexados (`rd_vk_nofans`) e o depth bias
    passou a ser emulado (`rd_vk_depthbias 0`). Pareceu ajudar, mas um
    rastreamento travou num desenho comum (o 114º depois de 113 iguais) e outro
    não travou: **não determinístico**.
  - Kit com Mesa 26.2.2, 26.2.3 e 26.2.4: todos travavam igual, então **não era
    regressão do driver**.
- **Causa real:** a GPU ficava presa num desenho indexado comum, sintoma de um
  shader que não termina. O cabeçalho da lista de luzes dinâmicas
  (`lights[uLightIndex]`) era lido como lixo no hardware e os laços de luz
  viravam praticamente infinitos. O Ryujinx zera a memória e por isso não
  reproduzia.
- **Correção (própria, sem copiar o NX-Modern):** `getLightRange()` em
  `main.fp` valida o cabeçalho (início 0, fins em ordem, limite de luzes, NaN
  rejeitado). Se for implausível, a superfície fica sem luz dinâmica naquele
  frame. 8º teste: **sem travamentos.**
- **Lições:**
  - O emulador não reproduz falhas de GPU nem lixo de memória.
  - Uma "melhora" depois de um contorno pode ser coincidência num problema não
    determinístico.
  - Os contornos `rd_vk_nofans 1` e `rd_vk_depthbias 0` ficaram como padrão.
  - O `rdoom_debug.txt` no SD permite testar variáveis do driver e CVARs sem
    novo build (ver `rdoom_debug.exemplo.txt`).

## D. Fluidez no console

### P10 — Frames de 24–31 ms com GPU em 8–13 ms
- **Causa:** CPU e GPU em série: um único submit no fim do frame, preso ao
  semáforo da imagem da swapchain.
- **Como foi achado:** timestamps de GPU no `perf.log` e benchmark no console
  (`rd_gpubench=1`, resultado em `logs/gpu-bench.txt`).
- **Correção:** a cena vai para a GPU ao terminar e em blocos de 300 desenhos
  (`rd_vk_earlysubmit`); só pós-processamento e 2D esperam o vsync. Também:
  dois frames em voo (regiões de buffer por frame), cena RGBA8
  (`rd_vk_scenefmt 2`), fim da cópia da cena (`rd_vk_sceneswap`).
- **Lição:** "espera pelo frame anterior" não é tempo de GPU; confie nos
  timestamps.

### P11 — Picos de 100–350 ms na primeira porta, tiro ou explosão
- **Causas:** pipelines compilados de novo a cada sessão (o cache só era salvo
  saindo pelo menu) e escritas síncronas no SD dentro dos frames: logs,
  `engine-session.txt`, `boot.log` a cada pipeline, stderr do mpg123.
- **Correção:** thread de escrita em segundo plano; cache de pipelines salvo
  3 s após o último pipeline novo.

### P12 — Cada pipeline novo custa ~20 ms no console (1,6 ms no Ryujinx)
- **Causa:** mesmo com acerto no cache, o driver envia o shader à memória da GPU
  e espera.
- **Correção:** lista de pipelines usados (`cache/uzdoom/rdoom-pipelines-all.bin`)
  recriada durante o carregamento do mapa. Como a pasta `cache/` some quando o
  dono atualiza copiando a pasta inteira, o pacote traz
  `rdoom-pipelines-seed.bin` (gerado por `scripts/merge-pipelines.py`) e a
  lista fica em `assets/`.
- **Lição:** a cada mod novo testado, juntar os pipelines do teste à semente.

### P13 — Explosões: frames de 100–200 ms de GPU
- **Causa:** preenchimento de dezenas de sprites translúcidos grandes de fumaça
  e fogo (400–650 sprites, até 430 translúcidos). Os sprites são iluminados na
  CPU, então o custo de GPU é a mistura.
- **Correção:**
  - resolução dinâmica da cena 3D (`rd_dynres`, mínimo 60%), com a arma e o HUD
    em resolução cheia e ampliação por desenho (`shaders/pp/upscale.fp`), porque
    o `vkCmdBlitImage` custava 2–4 ms no NVK;
  - descarte de fragmentos abaixo de 2% de alfa (`rd_sprite_alphacut`);
  - fumaça quase invisível não é desenhada (`nx_effect_minalpha 0.04`).

### P14 — A primeira explosão sempre engasga
- **Causa:** cada sprite novo era lido do SD e decodificado (PNG) **duas vezes**
  na thread do jogo: no recorte de bordas (`TrimBorders`) e no envio.
- **Correção:**
  - decodificação e recorte nos workers (`vk_asynctex.cpp`,
    `hw_nxprefetch.cpp`, `nx_jobs.cpp`; `rd_vk_async_textures`,
    `rd_async_sprites`), com uma decodificação por imagem;
  - o ator mantém o quadro anterior até o novo ficar pronto;
  - pré-carga das classes de ator que aparecem no nível;
  - lista de texturas de efeito por jogo (`rdoom-textures-*.txt`) carregada no
    início do mapa.

### P15 — Fumaça densa e colada na câmera
- **Correção:**
  - estabilizador (`nx_stabilizer 60`, `nx_governor.cpp`) que corta a fumaça
    primeiro e as luzes só a partir do nível 3;
  - orçamento de área de tela para efeitos translúcidos
    (`HWSprite::NXEffectOverBudget`), cortando de perto para longe;
  - projéteis sem dano (faíscas, destroços do Brutal) contam como efeito.
  Frames acima de 18 ms nas lutas caíram de 58% para ~25%.

### P16 — Engasgos de 90–155 ms sem custo de cena
- **Causa:** a thread de fundo segurava o lock do log (`fflush` de stdout e do
  `perf.log`) durante a escrita no SD, e qualquer linha de log do jogo esperava.
- **Correção:** os `FILE` do jogo são redirecionados para a memória
  (`RDoom_BufferFile`) e o descritor é escrito sem lock compartilhado.

### P17 — Outros travamentos ligados ao SD
- Frame de 2,4 s: o mutex de entradas do ZIP (`FZipFile::SetEntryAddress`) era
  mantido durante a leitura do SD. O cabeçalho agora é lido fora do mutex.
- "save" de 57 ms no `BeginFrame`: a lista de texturas era montada com
  `FString::AppendFormat`, em custo quadrático. Agora é formatada e gravada por
  um worker, 1 s após a primeira entrada nova e depois no máximo a cada 20 s.
- "save" de 67–83 ms: `M_GetCachePath(true)` fazia `mkdir` no SD a cada
  gravação. Os caminhos agora são calculados uma vez.

### P18 — Fumaça ainda pesada → camada de meia resolução
- **Correção:** efeitos translúcidos grandes vão para uma camada RGBA8 de meia
  resolução, misturada sobre a cena antes da arma e do HUD, quando somam
  ≥ 0,6 tela (`rd_halfeffects_minscreen`). A profundidade vem de uma cópia R32F
  da cena. A camada confere a si mesma: se a leitura de volta só der zero, ela
  desliga na sessão. **No Ryujinx ela sempre desliga**, porque o emulador não
  amostra visões de profundidade do NVK.
- **Problema derivado:** o dono copiou só o `RDoom.nro` e o jogo morria com
  "Unable to load shaders/pp/nxdepth.fp" (o `uzdoom.pk3` era antigo).
  Correção (30e33a509): a camada verifica os próprios shaders e se desliga em
  vez de parar o jogo.
- **Lição:** todo lump novo no `uzdoom.pk3` precisa ser opcional ou verificado
  em tempo de execução, e o dono deve ser avisado para copiar o pk3 junto.

### P19 — Frame de 40–65 ms a cada 10 s e "save" de 100–150 ms
- **Causa:** a thread de gravação rodava com prioridade mínima **no núcleo do
  jogo**; quando era interrompida segurando um lock, o jogo esperava. Também
  havia uma linha de relatório a cada 600 frames escrita na thread do jogo.
- **Correção:** thread de gravação no núcleo 2 com a prioridade do jogo
  (`0x2C`); linhas do relatório escritas pela thread de fundo.
- **Lição:** o jogo roda no núcleo 0 com prioridade `0x2C`. Os workers ficam nos
  núcleos 1 e 2 com `0x3B`, a prioridade mais baixa. Threads criadas sem núcleo
  explícito caem no núcleo 0.

### P20 — Hordas: tic de 16–25 ms
- **Causa:** `malloc` da newlib custa ~47 µs por objeto de ~1,6 KB; os imps do
  Brutal criam atores de hitbox o tempo todo.
- **Como foi achado:** horda repetível no Ryujinx
  (`test-env/content/rdtest-horde.pk3`, 150 imps, modo deus) e contadores de
  CPU por tic no `perf.log`.
- **Correção:** alocador próprio (`common/objects/nx_objpool.cpp`, listas livres
  por classe de 16 bytes em blocos de 128 KB) para objetos de script e para
  `malloc`/`free`/`realloc`/`calloc` de até 1 KB. Tic da horda 16–25 → 11–12 ms;
  heap da thread do jogo 2,8 → 0,4–0,6 ms por quadro.
- **Armadilha:** um `thread_local` dentro do `malloc` interceptado travou a
  inicialização; use `threadGetCurHandle`.

### P21 — 3–4 ms por quadro não medidos em cenas pesadas
- **Causa:** `r_ticstability`, padrão do UZDoom, gira em espera ocupada pelo
  tempo do último tic nos quadros sem tic.
- **Correção:** desligado no Switch (revisão 3 das configurações,
  `rd_defaults_rev`).
- **Lição:** o `.ini` salvo guarda os padrões antigos. Para mudar um padrão de
  CVAR arquivada é preciso migrar via `rd_defaults_rev`.

### P22 — Resolução dinâmica presa em 60% ou caindo sem motivo
Foram várias iterações:
- a ampliação por blit custava caro (→ ampliação por desenho);
- os timestamps incluíam pausas entre envios (→ guiar por vsync perdido e espera);
- com vsync o quadro oscila entre 16 e 18 ms e a regra de subida nunca era
  cumprida (→ decidir pelo tempo da GPU: sobe se a próxima escala cabe em 85%,
  cai acima de 90%);
- a regra antiga por tempo do quadro voltava enquanto a média recomeçava
  (→ nunca voltar a ela depois de ver timestamps; média recomeça a cada mapa;
  8 amostras);
- os timestamps subestimam a carga em Ashes/Compendium (→ também desce por
  média sustentada: quadro acima de 1,04× o orçamento e espera acima de 4 ms,
  incluindo o acquire; sobe só abaixo de 1,02×).
- **Lição:** o código está em `VulkanRenderDevice::UpdateDynamicResolution`
  (`vk_renderdevice.cpp`). Cada regra nova foi medida no console; mudar com
  cuidado.

### P34 — Crash na inicialização ao declarar uma CVAR à mão
- `extern FBoolCVar x;` escrito à mão derrubou o jogo: as CVARs são globais
  `F<Tipo>CVarRef`. Use `EXTERN_CVAR(Bool, x)`.

## E. Versões publicadas (1.0.0 → 1.1.4)

### P38 — Link sem OpenGL (1.0.0)
- Ao desligar `HAVE_OPENGL`, faltaram `gl_multisample`, `gl_dither_bpc`,
  `UpdateVRModes` e `PrintVRAM`. `hw_noopengl.cpp` fornece esses símbolos.
  `sdlglvideo.cpp` força Vulkan, e o caminho GL ficou sob
  `#if defined(HAVE_OPENGL) || defined(HAVE_GLES2)`.

### P23 — Visor do capacete do PB: 2D é o maior custo de GPU (1.1.0)
- **Causa:** o HUD do PB cobre a tela várias vezes (~1,9 ms de GPU por tela de
  área 2D) com o shader completo.
- **Correção:** `EFF_SIMPLE2D`, só o caminho 2D do `main.fp`
  (`#define SIMPLE_2D`), para imagens 2D com o material padrão; idêntico pixel a
  pixel. Imagens 2D totalmente transparentes não são desenhadas.
  2D/present 4,3–5,5 → 3,7–4,9 ms.
- **Bug no caminho:** crash ao iniciar, porque a tela de carregamento desenha
  2D antes de os shaders de efeito existirem. Só usar o shader quando
  `GetEffect(EFF_SIMPLE2D, ...)` não for nulo.

### P24 — Tela preta e crash ao sair (1.1.0)
- **Causa:** `nvk_FreeDescriptorSets` era chamado pela lista de exclusão de um
  frame ainda em voo, liberada **depois** de o gerenciador de descriptor sets
  destruir os pools.
- **Correção:** `~VulkanRenderDevice()` chama `mCommands->WaitForPendingFrame()`
  logo depois de `vkDeviceWaitIdle`.

### P25 — Travada de ~0,3 s ao pegar o traje antirradiação no PB
- **Causa:** a imagem de 1472×2160 da HUD era lida e decodificada na thread do
  jogo no frame em que aparecia.
- **Correção 1.1.0:** a lista de pré-carregamento passou a guardar imagens
  grandes da HUD (≥ 256×256).
- **Correção 1.1.4:** com o heap acima de 1,8 GB, o pré-carregamento era pulado
  inteiro (1.1.2) e o engasgo voltou (313 ms). Agora as imagens grandes da HUD
  são pré-carregadas mesmo assim (até 64 MB) e nunca são liberadas.

### P26 — WadFusion "não abre"
- O arquivo do dono foi gerado para o UZDoom 4.14 (108 erros de ZScript, os
  mesmos no UZDoom 5.0.3 de PC). Regerado com o WadFusion v1.6.1, funciona.
- "Não abre" depois disso era só a tela de título esperando um botão.
- O Legacy of Rust aparece no menu de episódios como "The Vulcan Abyss" e
  "Counterfeit Eden".

### P27 — Milhares de linhas do mpg123 gravadas no SD (1.1.1)
- Sons MP3 do PB com lixo no começo; cada aviso ia para `engine-stderr.log`
  durante o jogo. Correção: `MPG123_QUIET` em
  `libraries/ZMusic/source/decoder/mpg123_decoder.cpp`.

### P28 — Falta de memória após ~11 min de PB (1.1.2)
- **Sintoma:** `Could not realloc 24263712 bytes`.
- **Causa:** toda textura de sprite, gore e arma já vista ficava na GPU até o fim
  do mapa, e cada início de mapa carregava a lista de pré-carregamento inteira
  (2000 imagens acumuladas em quatro sessões). No Switch a GPU e o `malloc`
  dividem ~3,2 GB; o libnx entrega toda a memória livre ao heap.
- **Correção:**
  - liberação de texturas: com o heap em uso acima de `rd_vk_texevict_mb`
    (2300 MB), sprites, decalques e imagens da HUD sem uso há 30 s perdem a cópia
    na GPU e voltam quando reaparecem;
  - lista de pré-carregamento em ordem de uso, com orçamento de 256 MB
    (`rd_vk_texwarm_mb`), nenhum pré-carregamento acima de 1,8 GB;
  - `perf.log` mostra o heap a cada 5 s, e o erro de memória diz quem pediu
    (`m_alloc.cpp`).
- Na sessão seguinte, 26 min de PB + TNT sem falta de memória (pico ~2,37 GB).

### P29 — Project Brutality sem efeitos sonoros desde o primeiro dia (1.1.3)
- **Sintoma:** sem som de tiro, de monstros e de impacto (música tocava).
- **Causa:** o libsndfile da imagem Docker foi compilado **sem Ogg/Vorbis**. O
  `dkp-toolchain-common.cmake` do devkitPro força
  `CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE` e os portlibs só têm arquivos
  pkg-config, então os codecs não eram encontrados. Os 1433 sons Ogg do PB
  falhavam ao decodificar e eram marcados vazios.
- **Como foi achado:** contadores no `perf.log` de por que cada som não tocou
  (`#   sounds:` e `#   sound restarts:`).
- **Correção:** no `docker/Dockerfile`, antes de compilar o libsndfile: `sed`
  que desliga o `if (CMAKE_FIND_PACKAGE_PREFER_CONFIG)` em
  `cmake/SndFileChecks.cmake`, `PKG_CONFIG_LIBDIR` apontando para os portlibs do
  Switch e verificação de que `sndfile.pc` cita vorbis. O motor liga
  `vorbisenc vorbis FLAC opus ogg`.
- **Lição:** verificar recursos opcionais das dependências do toolchain; a
  ausência pode ser silenciosa.

### P30 — Travamentos de 0,8–1,2 s a cada 30 s (1.1.4)
- **Sintoma:** "freeze de meio segundo em alguns trechos" e quedas que não
  existiam antes, depois que os sons voltaram.
- **Causa:** a própria liberação de texturas da 1.1.2. No PB o heap fica perto
  de 2,3 GB, então ela rodava a cada 30 s, destruindo milhares de imagens de uma
  vez (e esperando a GPU e refazendo todos os descritores). Destruir uma imagem
  custa ~0,23 ms no console: 845 ms para 3682 imagens, pagos no início do frame
  seguinte (coluna `pending` do `perf.log`).
- **Correção:** fila em ordem de "mais antiga primeiro", liberando
  `rd_vk_texevict_perframe` (4) por quadro, sem esperar a GPU e sem
  `ResetHWTextureSets`. As imagens vão para a lista de exclusão do frame atual,
  destruídas quando a GPU termina. Imagens usadas por outro material (camada de
  brightmap, duas texturas com a mesma imagem) são registradas em
  `VkDescriptorSetManager::AddMaterial` e não são liberadas. Ryujinx: 6065
  imagens em 25 s, nenhum quadro acima de 30 ms (1.1.2: 223–235 ms por passada).
- **Lição:** liberar recursos de GPU sempre aos poucos; medir custo por objeto
  no console.

### P31 — Queda de fps com fumaça, mísseis e inimigos explodindo (sem solução)
- **Diagnóstico (logs 1.1.3, Master Levels + PB):** GPU em 10–13 ms por quadro,
  abaixo dos 16,7 ms. O limite é a **CPU**:
  - lógica do PB de 8–12 ms por quadro nas lutas (~400 `PB_BloodSpot`,
    ~140 `NashGoreGibs`, fumaça de barris);
  - ações de script isoladas de 13–22 ms (`A_Explode` de granada e foguete,
    gibs do Imp; notas "slow action" no `perf.log`).
- O corte de efeitos colados na câmera da 1.1.3 (`rd_fx_nearcull 20`,
  `rd_fx_nearfade 48`) não resolveu, porque não era GPU.
- A mixagem de áudio (SDL2 → OpenAL Soft) roda com prioridade `0x3B` e não
  interrompe o jogo; foi descartada como causa.
- O que ajuda o jogador: reduzir sangue e gore nas opções do próprio PB.

## F. Infraestrutura, testes e processo

### P39 — CI no GitHub Actions
- Faltava o Pillow (a arte do launcher lê imagens do Freedoom): `python3-pil`.
- Avisos de Node 20: actions atualizadas (checkout v7, cache v6,
  upload-artifact v7).
- O SDK do Mesa vem do release espelho `deps-mesa-26.2.4-606441c9`
  (`scripts/build.sh` baixa dele primeiro, o artefato original fica de
  reserva).

### P40 — Push recusado
- O dono edita o README no GitHub. Sempre `fetch` + `merge origin/main`
  mantendo as linhas dele. Ele também corrigiu o texto de custo, que não deve
  ser reescrito.

### P41 — Erros TLS "unexpected eof" no push/fetch
- Rede do dono instável: repetir em laço.

### P42 — Erros de teste no Ryujinx que custaram tempo
- O warp do launcher só aceita `MAPxx`/`ExMy`. Um perfil com warp `LR_MAP08` era
  recusado e a "sessão longa" rodou no mapa de título. Use `map lr_map08` num
  `autoexec.cfg`.
- No `vpad.py` o botão é `DOWN`, não `DDOWN`.
- `scripts/test-profile.py 'cvars={...}'` **substitui** o dicionário inteiro.
- O Ryujinx mostra chão preto, arma ausente ou tela preta com ou sem as mudanças;
  não serve para validar visual dessas partes.

### P35 — `DOOM2.wad` e mods nunca no repositório
- O `.gitignore` cobre. Antes de cada commit, conferir
  `git diff --cached --name-only`.

### P43 — Commits do aider (2026-10-18/20)
- O dono testou o aider (Ollama, qwen2.5-coder) e ficaram dois commits locais
  que se anulavam, além de arquivos `.aider*`. A pedido dele: a linha `.aider*`
  saiu do `.gitignore`, os arquivos saíram da pasta, o histórico do `main` foi
  reescrito sem os commits e a tag `v1.1.4` foi movida (o conteúdo da árvore é
  idêntico).

### P44 — `scripts/test.sh` falha com "host test build FAIL"
- Em 2026-10-20 o `ninja` não estava mais instalado no host
  (`build/test-results/host-configure.log`: "Running '/usr/bin/ninja' failed").
  O build do NRO usa o ninja do Docker e não é afetado. Reinstalar o ninja, ou
  rodar os testes com `cmake -G "Unix Makefiles"` num diretório separado
  (os quatro passos do `test.sh` passaram assim).
