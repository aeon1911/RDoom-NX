# Desempenho — RDoom NX

> **Todas as medições abaixo são do emulador Ryujinx 1.3.3** (host: Intel
> i5-9600KF, AMD Radeon RX 9070 XT/RADV, CachyOS). Elas mostram o custo
> relativo de CPU/driver de cada caminho, não os FPS do console. **Medições
> em Switch físico: pendentes.** Nenhuma medição usa overclock: o RDoom não
> mexe em clocks.

## Método

- NRO empacotado instalado no SD de teste, aberto pelo launcher com o
  controle virtual. Perfis de teste: `freedoom2-vulkan-stock` (Freedoom 2
  0.13.0) e `brutal-doom` (Brutal Doom v22 test 6 sobre DOOM II).
- `scripts/bench-ryujinx.sh <perfil> <renderizador> <rótulo> MAP01 90` —
  **sem limite** (vsync do perfil desligado, `vid_maxfps 0`, Ryujinx sem
  vsync), parado na entrada do MAP01, 90 s. Lê os resumos de 10 s do
  `perf.log` (só janelas dentro do nível, descartando a primeira).
- `scripts/pacing-ryujinx.sh … MAP01 60` — **configuração padrão** (vsync,
  60 FPS, Ryujinx a 60 Hz), andando e girando; `nx_perflog 2` registra cada
  frame; relata média, desvio padrão, percentis e frames atrasados.
- Os dois scripts alternam o automapa uma vez depois que o nível aparece e
  conferem que a captura não está preta (`image-check.txt`). Isso existe por
  causa do problema do OpenGL descrito em `RYUJINX_TEST_REPORT.md`: medições
  antigas do OpenGL com a vista preta foram marcadas com `INVALID.txt`.
- Mesma build em todas as comparações finais: `RDoom.nro` sha256
  `ff810453c23a7b3c…` (`evidence/*/…/nro.sha256`).

## Resultado: Vulkan × OpenGL, sem limite de FPS

| Cena | Vulkan | OpenGL | Vulkan sobre OpenGL |
|---|---|---|---|
| Freedoom 2, MAP01 | **367,5 fps** (2,72 ms) | 169,6 fps (5,90 ms) | **+117%** |
| Brutal Doom, MAP01 | **291,6 fps** (3,43 ms) | 159,4 fps (6,27 ms) | **+83%** |

Pastas: `evidence/benchmarks/{fd2,brutal}-map01-{vulkan,opengl}-v3/`.

Por frame (médias do `perf.log`):

| | Vulkan FD2 | OpenGL FD2 | Vulkan Brutal | OpenGL Brutal |
|---|---|---|---|---|
| cena 3D | 1,0 ms | 3,4 ms | 0,8 ms | 2,6 ms |
| fim de frame (2D + submit + present) | 0,6 ms | 1,5 ms | 0,6 ms | 1,8 ms |
| tic (simulação) por frame | 0,0 ms | 0,1 ms | 0,2 ms | 0,3 ms |

No Vulkan o fim de frame se divide em: 1 submit (0,10–0,13 ms em
`vkQueueSubmit`), limpeza de cache 0,00, acquire 0,08, present 0,15–0,16,
espera pelo frame anterior 0,15–0,22 ms.

## Resultado: ritmo de frames no padrão (vsync, 60 FPS)

| Cena | Backend | Média | Desvio | p99 | Pior | Frames > 25 ms |
|---|---|---|---|---|---|---|
| Freedoom 2 MAP01 | Vulkan | 60,0 fps (16,67 ms) | 0,69 ms | 18,8 ms | 24,9 ms | 0 |
| Brutal Doom MAP01 | Vulkan | 60,0 fps (16,65 ms) | 0,85 ms | 18,9 ms | 24,8 ms | 0 |
| Freedoom 2 MAP01 | OpenGL | 59,0 fps (16,95 ms) | 12,08 ms | 18,3 ms | 662 ms | 4 (0,14%) |
| Brutal Doom MAP01 | OpenGL | 58,0 fps (17,24 ms) | 11,95 ms | 26,8 ms | 634 ms | 37 (1,26%) |

Pastas: `evidence/pacing/fd2-map01-vulkan` (sem alternar o automapa) e
`evidence/pacing/*-v2` (com a alternância). Na execução `fd2-map01-vulkan-v2`
a alternância do automapa trouxe shaders novos e alguns frames lentos de
compilação no emulador (pior 81 ms, 11 frames > 25 ms); os grandes picos do
OpenGL (600+ ms) são o primeiro frame 3D, com a compilação de shaders do Mesa
e do Ryujinx.

## Evolução do Vulkan (Freedoom 2 MAP01, sem limite)

| Etapa | NRO | FPS | Fim de frame |
|---|---|---|---|
| Antes das otimizações | `0680c6fb…` | 97,0 | 5,7 ms |
| Limpeza de cache só em memória cacheada | `cc3c6809…` | 177,4 | 1,4 ms |
| + sobreposição CPU/GPU entre frames | `cc3c6809…` (`rd_vk_frameoverlap 1`) | 200,8 | 0,5 ms |
| + fim das gravações de `phase.log` por frame | `ff810453…` | 361,7 / 367,5 | 0,6 ms |

Detalhes de cada otimização em `docs/VULKAN.md` §3. A remoção do
`phase.log` também beneficia o OpenGL/GLES (eram 5 aberturas/gravações de
arquivo no SD por frame); a medição antiga de OpenGL (144,5 fps, `0680c6fb`)
não tem captura e não é comparável.

## Laço do jogo e tics

- A simulação continua em **35 tics/s** (sem mudança de ticrate). A
  renderização interpola entre tics; o ritmo vem do vsync FIFO (swapchain do
  NVK com 3 imagens) e de `vid_maxfps`.
- O que foi medido e mudou no laço:
  - escrita de arquivo por frame (`NX_Phase`) — removida;
  - espera da GPU no fim de cada frame — passou para o início do frame
    seguinte (a simulação e a entrada do próximo frame correm em paralelo
    com a GPU);
  - limpeza de cache por submit — removida onde é desnecessária.
- O custo de tic é pequeno nestas cenas (0,4 ms/tic no Freedoom, 1,2 ms/tic
  com Brutal Doom e 476 thinkers). Por frame ainda há ~0,5 ms de eventos de
  entrada e ~0,6 ms de atualização de música (`S_UpdateMusic`), constantes.
- O "estabilizador" herdado (`nx_stabilizer`, desligado por padrão) agora
  conta as esperas do Vulkan (acquire/present/frame anterior) como espera, e
  não como trabalho.

## O que ainda falta medir

- **Tudo em hardware real** (portátil e dock): FPS, ritmo de frames,
  consumo de memória em modo título e applet.
- Cenas pesadas de verdade (muitos monstros/efeitos, ex.: Brutal Doom em
  mapas de combate, Lexicon/Compendium em hubs grandes).
- `perf.log` em hardware: o arquivo é gravado em `sdmc:/switch/rdoom/perf.log`
  com resumos a cada 10 s; basta enviá-lo depois de jogar.
