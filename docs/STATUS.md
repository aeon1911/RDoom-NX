# RDoom NX — estado em 2026-10-13

Pacote atual: `dist/RDoom.nro` sha256 `f78662e024517fb24151b053f0dd7085e784294f7190ce7ffe6c7e6865d46a24`
(motor `5.0.3-55-g23e731af9`), zips e `SHA256SUMS.txt` em `dist/`.

## Feito e verificado

- Build reproduzível em Docker; série de patches em `engine/` (branch `rdoom-nx`) e em `patches/`.
- **Vulkan (padrão) funcionando no Ryujinx**: instância, superfície VI, NVK GM20B, swapchain,
  frames apresentados, mapas renderizados corretamente — Freedoom 2, Brutal Doom, Lexicon e
  Compendium. Sem fallback silencioso para OpenGL.
- **Desempenho Vulkan** (Ryujinx, sem limite): 97 → 367,5 fps no Freedoom MAP01; Brutal Doom
  291,6 fps; OpenGL 169,6 / 159,4 fps nas mesmas cenas. No padrão (vsync 60): 60,0 fps com desvio
  < 1 ms. Ver `PERFORMANCE.md`.
- Otimizações: limpeza de cache só em memória cacheada, sobreposição CPU/GPU entre frames, fim das
  gravações de `phase.log` a cada frame (todas com medição antes/depois).
- Launcher (perfis, bibliotecas, ordem de carga, editor, diagnóstico): 213 testes de host
  (ASan/UBSan), smoke de UI, verificação de pacote e de ausência de overclock — `scripts/test.sh`.
- "Contaminação de perfil" do teste antigo do Brutal Doom: era o roteiro de teste, corrigido
  (`PROFILE=` no `ryujinx-test.sh`).
- WadFusion: arquivo do usuário feito para UZDoom 4.14 (mesmos 108 erros no UZDoom 5.0.3 oficial
  de PC); o launcher identifica e explica.
- Documentação: README, PORTING_AUDIT, ARCHITECTURE, VULKAN, PROFILES, MOD_COMPATIBILITY,
  RYUJINX_TEST_REPORT, PERFORMANCE.

## Primeiro teste em Switch físico (2026-10-11, pelo autor)

- Vulkan inicia no console (NVK GM20B, `nv_compat: inactive`, memória `0:LVCa 1:LVCo`) e o jogo
  ficou **muito fluido** com DOOM2 + Trailblazer.
- **Falha:** `Could not submit command buffer: device lost` (a GPU caiu) — primeiro ao atirar e,
  nas sessões seguintes, já ao entrar no mapa (DOOM2 puro, Brutal, Legacy of Rust, Lexicon ao
  escolher um mapa). No Ryujinx o mesmo cenário funciona (o emulador não reproduz falhas da GPU).
- A piora entre sessões com o mesmo build aponta para caches gravados no SD (pipeline do UZDoom,
  gravado ao sair inclusive após o erro, e cache de shaders do Mesa em `sdmc:/.mesa`). Mudanças:
  o cache não é mais gravado depois de uma perda da GPU, a sessão seguinte apaga os caches
  automaticamente (`logs/gpu-lost.txt` → `gpu-lost-previous.txt`), há a ação "Limpar caches de
  shaders" (X) no Diagnóstico e o arquivo `rdoom_debug.txt` para testes sem novo build.
- Segundo teste (build af152e499): com caches limpos e `rd_vk_frameoverlap=0` a GPU ainda cai no
  1º frame 3D (menus 2D funcionam); com `NVK_DEBUG=push_sync` a GPU trava. Sobreposição e cache
  descartados. Suspeitos: profundidade/Z (compressão, ZCULL) ou outro estado 3D só do hardware.
  Build f7bb32d2d: mensagens do driver no log (`VK_EXT_debug_utils`), contornos automáticos em
  passos após cada perda da GPU (`gpu_workaround` no relatório) e logs anteriores em `logs/anterior/`.
- Terceiro teste (build f7bb32d2d): o driver registrou `channel 2 lost: fifo idle timeout` — a GPU
  trava no 1º frame 3D (não é falha de MMU). Os contornos `no_compression`, `NVK_SWITCH_NO_UBO_CBUF`
  e `NVK_SWITCH_MAPPED_COMPLETION=0` (um a um, cumulativos) não resolveram. Memória de comandos já é
  sem cache por padrão no SDK. Build 3ee01de29: passo 1 = todos os contornos + profundidade D32S8;
  passo 2 = rastreamento por desenho (`logs/boot.log`) para identificar o comando que trava.
- **Causa encontrada (4º teste, dump do push buffer com `push_sync`):** o envio que trava a GPU é
  um único desenho de 4 vértices entre `LOAD_ZCULL` e `STORE_ZCULL` — o primeiro desenho que usa o
  buffer de profundidade da cena (frames só 2D nunca travaram). O NVK só liga ZCULL para imagens de
  profundidade com uso dentro de {transfer, sampled, depth attachment, input attachment}; no build
  790a58706 as profundidades no Switch são `TRANSIENT_ATTACHMENT` (só anexo), então o ZCULL fica
  desligado (`vulkan_scene_depth: … ZCULL off`). SSAO fica bloqueado enquanto isso; `rd_vk_zcull=1`
  volta ao comportamento antigo para teste. **Resultado no console: ainda trava** — o dump no
  Ryujinx confirma que o ZCULL saiu (sem `LOAD/STORE_ZCULL`), então ele não era a causa.
  Build 9eb182e9a: escada de experimentos — 1 = fans como triângulos, 2 = + sem escrita de profundidade/
  stencil, 3 = + sem testes de profundidade/stencil, 4 = rastreamento (agora também no wipe).
- **5º teste (escada de experimentos):** com "triangle fans" desenhados como triângulos indexados o
  console **entra no mapa**; o travamento seguinte veio no primeiro tiro, nos primeiros pipelines com
  *depth bias* (marcas de bala). Build 58637c0cf: fans → triângulos e depth bias emulado pelo viewport
  ligados por padrão; a próxima perda da GPU vai direto ao modo rastreamento.
- **6º teste:** no modo normal ainda trava ao entrar; no rastreamento um teste travou num desenho
  comum (o 114º, depois de 113 iguais) e outro rodou sem travar → o travamento não depende de um
  desenho específico. Hipótese atual: regressão no driver. O GZDoom-NX-Modern (funciona no console)
  embute **Mesa 26.2.1 (git-fe913078f1, 11/09)** do mesmo autor (NaGa); o nosso SDK é 26.2.4 (06/10).
  Kit `dist/RDoom-teste-driver.zip`: o mesmo build ligado a Mesa 26.2.2 (25/09, run 36188294344),
  26.2.3 (28/09, run 36439383707) e 26.2.4, para localizar a versão que introduziu o problema.
  Imagens Docker variantes: `third_party/mesa-sdk/dl/Dockerfile.sdk2622/.sdk2623` (hashes dos
  arquivos em `dl/*.files.sha256`). Logs das últimas 5 sessões em `logs/anterior … anterior-5`.
- **7º teste (kit de drivers, 2026-10-12):** Mesa 26.2.2, 26.2.3 e 26.2.4 travam do mesmo jeito ao
  entrar no MAP01 (DOOM2 + Trailblazer, `channel 2 lost: fifo idle timeout`) → não é regressão do
  driver depois de 25/09. O dump do 6º teste mostra a GPU presa num draw indexado comum (macro MME de
  draw, 30 índices): típico de shader que não termina. Causa provável: o cabeçalho da lista de luzes
  dinâmicas (`lights[uLightIndex]`) lido como lixo no hardware, transformando os laços de luz em
  trabalho praticamente infinito (o tiro cria a luz do clarão; o Ryujinx zera a memória e não
  reproduz). O mesmo travamento foi relatado no GZDoom 4.14 com esse driver.
  Build beafab8d9: `getLightRange()` em `main.fp` valida o cabeçalho (0, fins em ordem, até 256 luzes;
  NaN rejeitado) e, se for implausível, a superfície fica sem luz dinâmica naquele frame — os laços
  ficam sempre limitados. Custo: uma comparação por superfície iluminada. Ryujinx: 60 fps, clarão do
  tiro visível no Brutal Doom (`evidence/ryujinx/69-lightguard-brutal`). Build 8e8749d55: uma perda
  de GPU deixada por build antigo limpa os caches mas não avança a escada (`gpu-lost.txt` tem `build:`).
  Kit de drivers arquivado em `build/kits/`.
- **8º teste (build 8e8749d55, 2026-10-12): sem travamentos** com Trailblazer e Brutal Doom 21 —
  a validação do cabeçalho de luzes resolveu o `device lost`. Logs em `evidence/hw/2026-10-12-lightguard`.
  O `perf.log` real mostra o jogo **limitado pela GPU**: 32–41 fps no MAP01 com Trailblazer, CPU com
  1,5–3 ms de cena e 1,3 ms de tic, e ~18–23 ms por frame esperando a GPU. Build 3433686d6: tempos de
  GPU por frame no `perf.log` (cena 3D / pós-processamento / 2D+apresentação, modo portátil/dock),
  benchmark no console (`rd_gpubench=1`, resultado em `logs/gpu-bench.txt`), formato da cena
  configurável (`rd_vk_scenefmt`) e fim da cópia da cena para o pós-processamento (`rd_vk_sceneswap`).
- **Benchmark no console (Lite, portátil, Brutal Doom 21 MAP01):** GPU 6,75 ms/frame no ponto
  congelado; luzes dinâmicas ~0%, escala 0,75 −36%, escala 0,5 −62%, RGBA8 −4%, RGB10A2 −2%, ZCULL 0%,
  a cópia antiga da cena +8% (removida). Durante o jogo a GPU leva 8–13 ms, mas os frames levavam
  24–31 ms: a GPU não é o gargalo, CPU e GPU trabalhavam em série (um envio por frame, preso ao
  semáforo da imagem da tela). Build 6b42e83a0: a cena vai para a GPU ao terminar e em blocos de
  300 desenhos (`rd_vk_earlysubmit`, `vk_submit_size 300`); só pós-processamento/2D esperam o vsync.
  Logs em `evidence/hw/2026-10-12-gpubench`.
- **Teste do 6b42e83a0 (Brutal Doom 21):** GPU 10–14 ms, frames ainda 20–31 ms (CPU esperava o frame
  anterior antes de montar o próximo) e picos de 100–350 ms na primeira porta/tiro/explosão. Causas:
  pipelines compilados de novo a cada sessão (cache só era salvo saindo pelo menu) e escritas síncronas
  no SD dentro dos frames (logs, `engine-session.txt`, `boot.log` por pipeline, stderr do mpg123).
  Build 273d38b21: dois frames em voo (buffers por frame em duas regiões), escrita dos logs por uma
  thread em segundo plano, cache de pipelines salvo 3 s após o último pipeline novo, `perf.log` em
  `logs/` com contagem de pipelines e texturas por frame lento. Logs em `evidence/hw/2026-10-12-earlysubmit`.
- **Teste do 273d38b21 (duas sessões curtas, Brutal Doom 21):** espera pelo frame anterior caiu de
  11–15 ms para 6,6–7,8 ms; cache de pipelines carregado (297 KB), mas cada pipeline ainda custa ~20 ms
  no console (1,6 ms no Ryujinx): o driver envia o shader à memória da GPU e espera. Build c391ba597:
  lista de pipelines por jogo (`cache/uzdoom/rdoom-pipelines-<hash>.bin`) recriada no carregamento do
  mapa. Logs em `evidence/hw/2026-10-12-pipelined`.
- **Teste do c391ba597 (Brutal Doom 21):** pré-criação funcionou (`40 of 42 pipelines in 1099 ms`, nenhum
  pipeline novo no jogo). Restam picos de GPU nas explosões (dezenas de sprites translúcidos grandes de
  fumaça: cena 3D > 16 ms, frames de até ~90 ms) e cenas pesadas a 44 fps (GPU 14 ms). Build 4cd05c98b:
  resolução dinâmica da cena 3D (`rd_dynres`, meta 12 ms de GPU, mínimo 60%, ampliação linear antes da
  arma/HUD, que ficam em resolução cheia). Logs em `evidence/hw/2026-10-12-warmup`.
- **Teste do 4cd05c98b:** "muito mais fluido"; 60 fps cravados fora das explosões. A resolução dinâmica
  ficou presa em 60% (o blit de ampliação custava 2–4 ms no NVK e impedia a volta) e as explosões ainda
  davam frames de 100–200 ms de GPU (400–650 sprites, até 430 translúcidos; sprites são iluminados na
  CPU, então o custo é mistura). Build 87e335d13: cena RGBA8 por padrão (`rd_vk_scenefmt 2`), descarte de
  fragmentos de sprites translúcidos abaixo de 2% de alfa (`rd_sprite_alphacut`), ampliação por desenho
  (`shaders/pp/upscale.fp`) e controle que volta a 100% quando cabe. Resumo em `evidence/hw/2026-10-12-dynres/RESUMO.txt`.
- **Teste do 87e335d13:** "100% fluido em grande parte"; a primeira explosão ainda cai mesmo reabrindo o
  jogo (texturas de fogo/fumaça enviadas na primeira aparição, 9–23 por frame; listas de desenho de
  30–70 ms na CPU) e a segunda é fluida; resolução dinâmica presa em 60% porque os timestamps incluem as
  pausas entre envios. Build seguinte: lista de texturas de efeito por jogo
  (`cache/uzdoom/rdoom-textures-<hash>.txt`) carregada no fim do pré-carregamento do mapa; resolução
  dinâmica guiada por frame atrasado + espera pela GPU.
- **Teste do 35cd95841 (2026-10-13):** "um pouco mais fluido", mas com quedas. Sessões de ~15 s: a lista
  de texturas só ia para o SD 3–5 s depois da explosão e se perdia ao sair (a 2ª sessão não pré-carregou
  nada). O frame da explosão gastou ~150 ms em "bsp/lists" lendo 3,5 MB do SD e decodificando PNG: cada
  sprite novo era decodificado duas vezes na thread do jogo/renderização (recorte de bordas em
  `TrimBorders` e depois o envio). Build ec326349f: decodificação e recorte no worker do núcleo 2
  (`vk_asynctex.cpp`, `rd_vk_async_textures`; `rd_async_sprites`), sprite fica de fora 1–2 frames até a
  imagem estar pronta, classes pré-carregadas aos poucos (0,75 ms/frame), sons com prioridade URGENT,
  lista gravada 1 s após cada entrada e escritor em 0,25 s. perf.log ganhou jogador/spawn/StartSound e
  notas de ações de script ≥ 5 ms (arquivo:linha). Logs em `evidence/hw/2026-10-13-texwarm`.
- **Teste do ec326349f (2026-10-13):** a lista funcionou (1ª sessão gravou 429, 2ª pré-carregou 429 em
  0,8 s). Problemas: ao abrir uma porta os sprites piscaram e levaram ~1 s para aparecer (um worker,
  duas decodificações por imagem); 2ª sessão com frame de 132 ms (21 texturas de parede/piso: `gl_precache`
  estava desligado) e vários de 90–155 ms sem custo de cena — a thread de fundo segurava o lock do log
  (`LogFlush`/`fflush` de stdout e perf.log) durante a escrita no SD e qualquer linha de log do jogo
  esperava (ex.: `vulkan_frames_presented` a cada 600 frames); resolução dinâmica presa em 60%.
  Build 8cba57262: logs do jogo só vão para a memória (gravação sem lock compartilhado), uma
  decodificação por sprite (recorte calculado no mesmo buffer), ator mantém o quadro anterior até o novo
  ficar pronto, 2 workers, `gl_precache` ligado sem os sprites de todas as classes (migração única de
  padrões salvos via `rd_defaults_rev`), resolução dinâmica sobe sem quadro perdido. Logs em
  `evidence/hw/2026-10-13-asynctex`.
- **Teste do 8cba57262 (2026-10-13):** portas melhores segundo o autor; nenhum frame > 90 ms na 1ª sessão
  de jogo, dois na 2ª (95–115 ms dentro de `BeginFrame`, com 4 envios de textura e só 5–37 ms de espera
  pela GPU). Fumaça do Brutal apontada como o peso restante (resolução dinâmica caiu a 60%). Build
  5625345a5: perf.log detalha o `BeginFrame` e o envio mais lento (nota para > 10 ms), resumo conta frames
  > 18 ms e > 35 ms, pré-cargas só sobem após frame em dia, `nx_effect_minalpha 0.04` (fumaça quase
  invisível não é desenhada). Logs em `evidence/hw/2026-10-13-hold`.
- **Teste do 5625345a5 (2026-10-13):** explosões e portas sem travar segundo o autor; a fumaça ainda pesa.
  Log: `BeginFrame` de 57 ms era a gravação da lista de texturas (FString montada linha a linha); um frame
  de 2,4 s na montagem de listas enquanto o SD gravava (o mutex de entradas do ZIP era mantido durante a
  leitura do SD); em lutas com fumaça metade dos frames atrasados com a GPU limitada por preenchimento
  mesmo a 60%. Build 8cf03687d: cabeçalho do ZIP lido fora do mutex, lista gravada 1 s após a primeira
  textura nova e depois no máximo a cada 20 s, estabilizador ligado (`nx_stabilizer 60`), menu/pausa não
  contam como atraso. Logs em `evidence/hw/2026-10-13-smoke`.
- **Teste do 8cf03687d (2026-10-13):** "fumaça continua igual". Em luta o estabilizador ficou no nível
  máximo e ainda havia 188 sprites translúcidos por frame, 58% dos frames atrasados, 9,6 ms de cena 3D na
  GPU a 60% (fumaça próxima e densa; cortes por distância/opacidade não alcançam). "save" no `BeginFrame`
  de 67–83 ms: `M_GetCachePath(true)` (mkdir no SD) a cada gravação e leitura do cache de pipelines.
  Build a7f213106: orçamento de área de tela para efeitos translúcidos sob o estabilizador (20 telas no
  nível 1 a 3 no nível 6; perto→longe, some a fumaça atrás de fumaça), caminhos das listas calculados uma
  vez, cache de pipelines lido num worker com mutex. Logs em `evidence/hw/2026-10-13-stab`.
- **Teste do a7f213106 (2026-10-13, sessão de ~50 s):** em luta, frames acima de 18 ms caíram de 58% para
  ~25% e os acima de 35 ms de 42–60 para 1–21 por janela de 10 s; translúcidos por frame de 188 para 62–122;
  nenhum frame ≥ 90 ms fora o carregamento. O estabilizador fica nos níveis 5–6 quase o tempo todo
  (luzes dinâmicas reduzidas a 2–3) e a resolução dinâmica em 60%. Logs em `evidence/hw/2026-10-13-area2`.
  Build fb774e06a: o estabilizador corta a fumaça primeiro (orçamento de área desde o nível 1) e as luzes só
  a partir do nível 3, devolve detalhes após 60 frames bons; perf.log com resumo a cada 5 s, efeitos
  cortados por frame e frames acima de 40 ms.
- **Teste do fb774e06a (2026-10-13, 80 s, MAP01–MAP02):** quase sempre 60 fps, 0–3 frames > 35 ms a cada 5 s;
  uma explosão deu ~25 frames de 41–69 ms (até 55 ms de espera pela GPU, ~300 translúcidos, só 17 cortados:
  faíscas/chamas/destroços do Brutal são projéteis); cada gravação da lista (a cada 20 s) ainda custava
  95–165 ms. Build 5.0.3-44-ge01b911d4: projéteis sem dano contam como efeito, lista formatada/gravada por um worker.
  Logs em `evidence/hw/2026-10-13-stab2`.
- **Teste do e01b911d4 (2026-10-13, 2,5 min):** travão da lista resolvido; nas explosões o orçamento corta
  até 84 efeitos/frame, 0–3 frames > 35 ms a cada 5 s. Os frames de 41–56 ms restantes vinham a cada 10 s:
  a linha de log do relatório de sessão (a cada 600 frames) custava 37–39 ms no present. O estabilizador nunca
  saía do nível 6 (CPU do Brutal acima de 2/3 do frame). Build 5.0.3-45-g4020ab81e: linhas do relatório escritas pela thread
  de fundo; detalhes voltam com trabalho < 80% do frame. Logs em `evidence/hw/2026-10-13-effects`.
  Atenção: rodar o Ryujinx com o cartão do Switch montado por MTP (DBI) travou o PC do autor.
- **Teste do 4020ab81e (2026-10-13):** estabilizador devolve detalhes (níveis 0–1 com 20–26 luzes nas partes
  calmas), explosões com 0–5 frames > 35 ms a cada 5 s. Restavam um frame de 40–65 ms a cada 10 s e "save"
  de 100–150 ms: a thread do jogo esperava um lock da thread de gravação, que rodava com prioridade mínima no
  núcleo do jogo e ficava atrás das outras threads quando interrompida segurando o lock. Build 5.0.3-46-gbda37a838 (b173c0a27 antes da troca do e-mail dos commits): thread de
  gravação no núcleo 2 com a prioridade do jogo; uma lista de pipelines para todos os jogos
  (`rdoom-pipelines-all.bin`). Logs em `evidence/hw/2026-10-13-sesslog`.
- **Teste do bda37a838 (2026-10-13):** Freedoom 2 + Brutal (60 s): um único frame > 40 ms (logo após entrar no
  mapa), janelas de 5 s com 0–1 frame > 35 ms, estabilizador no nível 0–1 com 25–26 luzes; o travão a cada 10 s
  e os "save" sumiram. DOOM2 + Brutal (2 min, com troca de mapa): pipelines prontos em 3 ms no carregamento,
  nenhum shader criado em jogo, poucos frames de 40–68 ms; numa luta pesada até 17 frames > 35 ms em 5 s.
  Logs em `evidence/hw/2026-10-13-writer`.
- **Fumaça e fogo em meia resolução (5.0.3-47-gced311a28, 2026-10-13):** efeitos translúcidos grandes
  (alfa ou aditivos, só efeitos puros, ≥ 0,4% da tela cada) vão para uma camada RGBA8 de meia resolução,
  misturada sobre a cena antes da arma e do HUD, quando somam ≥ 0,6 tela (`rd_halfeffects_minscreen`;
  abaixo disso a camada custaria mais do que economiza). A profundidade da camada vem da cena (cópia R32F
  em meia resolução + passada que grava `gl_FragDepth`). Nos primeiros quadros a cópia é lida de volta:
  se vier só zero a camada desliga na sessão (`vulkan_half_effects` no engine-session.txt) e os efeitos
  voltam a ser desenhados como antes. No Ryujinx é o que acontece: o emulador não amostra a visão de
  profundidade do NVK (`Invalid texture format 0x2512B`); camada, composição e mapeamento foram
  conferidos lá com os modos `rd_halfeffects_debug` 2–5 e 10 (`evidence/ryujinx/119`–`135`). perf.log
  mostra efeitos na camada e a área deles por quadro. **Teste no console pendente.**
- **Teste do ced311a28 (2026-10-13):** nenhum jogo entrava no mapa — erro fatal "Unable to load
  'shaders/pp/nxdepth.fp'": só o `RDoom.nro` novo foi copiado, o `uzdoom.pk3` do SD era o antigo. 30e33a509:
  a camada confere os próprios shaders e se desliga (linha `vulkan_half_effects` pede o pacote inteiro)
  em vez de parar o jogo; conferido no Ryujinx com um `uzdoom.pk3` sem o shader
  (`evidence/ryujinx/136-oldpk3`). Logs em `evidence/hw/2026-10-13-halffx`.
- **Teste do 30e33a509 (2026-10-13, Lexicon + Brutal e Freedoom 2 + Lexicon + Brutal):** a camada de meia
  resolução funciona no console (`vulkan_half_effects_depth: ok`); nas lutas ela recebe 4–13 efeitos por quadro
  (1,2–1,9 tela de fumaça) e os mapas ficam em 55–60 fps, poucos frames > 35 ms. Mas a vista ficou em 60% e o
  estabilizador no nível 6 o mapa inteiro, com a GPU em ~10 de 16,7 ms: com vsync o quadro oscila entre 16 e
  18 ms e as regras de volta (45/60–300 quadros seguidos abaixo de 17,2–17,3 ms) nunca se cumpriam. 5.0.3-49-g453febfd0:
  resolução dinâmica decide pelo tempo da GPU (sobe enquanto a próxima escala cabe em 85%, cai só com a GPU
  acima de 90%); estabilizador ignora quadros até 20% acima do alvo. O hub do Lexicon roda a ~33 fps (GPU
  presa, 3D 12,7 ms a 60%) — o autor pediu para deixar como está. Logs em `evidence/hw/2026-10-13-halffx2`.
- **Teste do 453febfd0 (2026-10-13, mapa de slaughter do Lexicon + Brutal):** 1000–1265 monstros, tic 6–9 ms,
  48–57 fps limitados pela CPU (GPU 8,5 ms); camada de meia resolução com 18–25 efeitos por quadro. A vista
  continuou em 60%: depois de cada mudança a média da GPU recomeça por 20 quadros e nesse intervalo a regra
  antiga (tempo do quadro) derrubava cada subida nos quadros de 20 ms da CPU. 5.0.3-50-g74c08b37a: com carimbos de tempo
  vistos a regra antiga nunca mais roda (a escala espera a média), a média recomeça a cada mapa; perf.log
  mostra também a escala máxima da janela. A primeira sessão começou sem cache de pipelines (pacote
  reinstalado): pipelines criados em jogo só nessa vez. Logs em `evidence/hw/2026-10-13-dyngpu`.
- **Teste do 74c08b37a (2026-10-13, slaughter do Lexicon + Brutal):** a vista agora sobe a 100% (GPU 9–15 ms).
  As quedas que restam são da CPU: 1700–2100 atores, tic de 16–25 ms; os imps do Brutal custam 5–11 ms por tic
  (cada um cria dois atores de hitbox a cada 6 tics) e cada tiro de espingarda 7–15 ms (A_Refire refaz a
  cadeia de 26 projéteis decorativos + 14 hitscans). O hub ficou lento nos primeiros segundos (escala descendo
  de 100% em passos de 20 quadros). E de novo ~30 pipelines criados no meio do jogo: a cada atualização a
  pasta `cache/` some do SD (sessões "anterior" sem cache algum). 5.0.3-51-gec2a43adb: decisões da resolução com 8
  amostras e descida dimensionada; o pacote traz `rdoom-pipelines-seed.bin` (81 pipelines das sessões de
  teste, `scripts/merge-pipelines.py`), juntado à lista do cartão — instalação nova no Ryujinx: 80 criados no
  carregamento, nenhum em jogo (`evidence/ryujinx/142-seed-fresh`). Logs em `evidence/hw/2026-10-13-dynhold`.
- **Hordas na CPU (5.0.3-52-g6bba77e33, 2026-10-13):** horda repetível no Ryujinx (`test-env/content/rdtest-horde.pk3`:
  150 imps do Brutal v21 em volta do jogador, invulnerável): colisão 0,8 ms e visão 0,05 ms por tic, mas criar
  os ~50 atores de hitbox por tic custava 2,5 ms, quase tudo no `malloc` da newlib (~47 µs por objeto de ~1,6 KB).
  Objetos de script agora vêm de um alocador próprio (listas livres por classe de 16 bytes em blocos de 128 KB,
  `common/objects/nx_objpool.cpp`): tic da horda 7,2 → 4,5 ms, criação 2,5 → 0,34 ms. perf.log ganhou por tic:
  colisão, visão, criação de atores por fase, partes da criação de objetos e malloc/free da thread do jogo
  por quadro (~650 mallocs, ~1 ms no Ryujinx; trocar o alocador global fica para depois de medir no console).
- **Teste do 6bba77e33 (2026-10-14, CSH101 do Lexicon slaughter + Brutal v21):** com o alocador de objetos o tic
  da horda caiu de 16–25 ms para 11–12,6 ms (37–55 fps, ~1800–2000 atores); criar ~75 atores por tic custa
  1,0–1,1 ms (0,2 de alocação); colisão 1,7–2,3 ms; imps 3,8 ms. A lista do pacote preparou 79 pipelines no
  carregamento (instalação do zero: até os perfis foram recriados); ainda 13 no hub e 6 no mapa. A thread do
  jogo gastava ~2,8 ms por quadro em ~1500 malloc/free da newlib. 5.0.3-53-gc5638cdba: malloc/free/realloc/calloc/
  malloc_usable_size de até 1 KB saem do mesmo pool (Ryujinx: 1,1 → 0,22 ms por quadro; 4 min de horda sem
  erro); lista do pacote com 95 pipelines (hub VR incluído). Logs em `evidence/hw/2026-10-14-objpool`.
- **Teste do c5638cdba (2026-10-14, seis sessões):** heap da thread do jogo 2,8 → 0,4–0,6 ms por quadro.
  Lexicon slaughter CSH201 (1500–1780 atores): 45–55 fps, tic 6,6–9,4 ms, quase nenhum frame > 35 ms
  (antes 23–36 fps). Aliens Eradication TC: 52–60 fps. Compendium e Ashes 2063: 43–53 fps em trechos, com a
  CPU esperando 13–15 ms por quadro pela GPU e os carimbos de tempo marcando 12–13 ms — a resolução não
  baixava. Ashes com os três episódios juntos: erro fatal de CVARINFO duplicado (os episódios são para
  carregar separados; mesmo erro no UZDoom de PC). 5.0.3-54-ga50efb888: médias de ~20 quadros também baixam a escala
  (quadros > 18,7 ms com espera pela GPU > 6 ms) e ela só sobe com 60 fps na média.
  Logs em `evidence/hw/2026-10-14-pool`.
- **Teste do a50efb888 (2026-10-14):** Ashes 2063 55–60 fps (resolução ~65% nos trechos pesados, volta a
  100%), Compendium NJZ01 54–60 fps (antes 43–53). No slaughter, 3–4 ms de cada quadro fora de tudo que o
  perf.log mede: `r_ticstability` gira, em cada quadro sem tic (25 de 60 por segundo), pelo tempo do último
  tic (~8 ms ali). 5.0.3-55-g23e731af9: desligado por padrão no Switch (revisão 3 das configurações desliga uma vez);
  horda no Ryujinx: tempo não medido 2,7 → 0,7 ms por quadro. Logs em `evidence/hw/2026-10-14-dynavg`.
- **Publicado em 2026-10-13:** https://github.com/aeon1911/RDoom-NX (público) com o motor em
  https://github.com/aeon1911/RDoom-NX-engine (fork do UZDoom, branch `rdoom-nx`, submódulo `engine/`).
  Autor dos commits trocado para o e-mail privado do GitHub (os hashes do motor mudaram). Não publicados:
  IWADs/mods, `test-env/`, `evidence/`, builds e SDKs baixados.
- Ryujinx: o escurecimento de paredes/HUD acontece também sem workers (`evidence/ryujinx/105-biglist-noworkers`),
  só depende do número de texturas criadas — limitação do emulador. Para validar visual no Ryujinx usar
  `rd_sprite_prefetch 0` e lista vazia (`evidence/ryujinx/106-hold-nopf`).
- Ryujinx: depois de algumas centenas de texturas enviadas (com ou sem a lista pré-carregada) paredes,
  pisos e partes do HUD ficam pretos — reproduzido sem a lista (`evidence/ryujinx/95-brutal-nolist`).
  Não aparece no console em sessões longas; atribuído ao cache de texturas do emulador (endereços de GPU
  reutilizados). O primeiro tiro no Ryujinx custa 40–60 ms em ações nativas variadas (tradução JIT do
  emulador; no console o tic da explosão foi 1,3 ms).
- Arte: ícone do NRO e fundo do launcher refeitos a partir de gráficos do Freedoom 2 (TITLEPIC,
  BOSSBACK, HEADA1, PLAYF1; licença BSD em `licenses/freedoom-art/`), gerados por
  `assets/src/generate_art.py` direto do `freedoom2.wad`; logotipo RDoom mantido.
- Ryujinx: Brutal Doom pelo caminho do título fica com tela preta no Vulkan (com ou sem ZCULL;
  pelo `warp` funciona) — problema separado, a investigar.
- Descartado pela desmontagem do driver: a remoção das limpezas de cache da CPU (memória coerente
  do NVK é mapeada sem cache no console).

## Pendente

- Teste no console do build atual: hub do Lexicon nos primeiros segundos, pipelines na primeira sessão
  depois de atualizar.
- Mapas com hordas (slaughter): medir no console o malloc em pool; depois colisão (1,7–2,3 ms por tic) e os
  scripts dos imps (3,8 ms por tic com ~150).
- Outros mods no console: Trailblazer ainda não testado; Compendium/Ashes testados (ver acima). Modo dock:
  não se aplica (Switch Lite). Opcional: launcher avisar quando mods selecionados declaram a mesma cvar.
- OpenGL no console: vista preta. No Ryujinx (OpenGL de desktop) a vista 3D fica preta ao começar direto
  num mapa até alternar o automapa.
- WadFusion: regerar `doom_fusion.ipk3` com o WadFusion v1.6.1 (precisa dos WADs do usuário).
- Release no GitHub (`.nro` + Freedoom, sem IWADs comerciais), CI e README em inglês.
- Sessões longas (1–2 h): memória e estabilidade.
- Depois: Project Brutality (mais pesado em scripts que o Brutal Doom).
- Lightmaps ainda sobem por cópia (compute) no Vulkan; nenhum mod testado usa.
