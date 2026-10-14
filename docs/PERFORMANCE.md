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
- `scripts/bench-ryujinx.sh <perfil> <rótulo> MAP01 90` —
  **sem limite** (vsync do perfil desligado, `vid_maxfps 0`, Ryujinx sem
  vsync), parado na entrada do MAP01, 90 s. Lê os resumos de 10 s do
  `perf.log` (só janelas dentro do nível, descartando a primeira).
- `scripts/pacing-ryujinx.sh … MAP01 60` — **configuração padrão** (vsync,
  60 FPS, Ryujinx a 60 Hz), andando e girando; `nx_perflog 2` registra cada
  frame; relata média, desvio padrão, percentis e frames atrasados.
- Os dois scripts alternam o automapa uma vez depois que o nível aparece e
  conferem que a captura não está preta (`image-check.txt`).
- Mesma build em todas as comparações finais: `RDoom.nro` sha256
  `ff810453c23a7b3c…` (`evidence/*/…/nro.sha256`).

## Resultado: sem limite de FPS

| Cena | Vulkan |
|---|---|
| Freedoom 2, MAP01 | **367,5 fps** (2,72 ms) |
| Brutal Doom, MAP01 | **291,6 fps** (3,43 ms) |

Pastas: `evidence/benchmarks/{fd2,brutal}-map01-vulkan-v3/`.

Por frame (médias do `perf.log`):

| | Freedoom 2 | Brutal Doom |
|---|---|---|
| cena 3D | 1,0 ms | 0,8 ms |
| fim de frame (2D + submit + present) | 0,6 ms | 0,6 ms |
| tic (simulação) por frame | 0,0 ms | 0,2 ms |

No Vulkan o fim de frame se divide em: 1 submit (0,10–0,13 ms em
`vkQueueSubmit`), limpeza de cache 0,00, acquire 0,08, present 0,15–0,16,
espera pelo frame anterior 0,15–0,22 ms.

## Resultado: ritmo de frames no padrão (vsync, 60 FPS)

| Cena | Média | Desvio | p99 | Pior | Frames > 25 ms |
|---|---|---|---|---|---|
| Freedoom 2 MAP01 | 60,0 fps (16,67 ms) | 0,69 ms | 18,8 ms | 24,9 ms | 0 |
| Brutal Doom MAP01 | 60,0 fps (16,65 ms) | 0,85 ms | 18,9 ms | 24,8 ms | 0 |

Pastas: `evidence/pacing/fd2-map01-vulkan` (sem alternar o automapa) e
`evidence/pacing/*-v2` (com a alternância). Na execução `fd2-map01-vulkan-v2`
a alternância do automapa trouxe shaders novos e alguns frames lentos de
compilação no emulador (pior 81 ms, 11 frames > 25 ms).

## Evolução do Vulkan (Freedoom 2 MAP01, sem limite)

| Etapa | NRO | FPS | Fim de frame |
|---|---|---|---|
| Antes das otimizações | `0680c6fb…` | 97,0 | 5,7 ms |
| Limpeza de cache só em memória cacheada | `cc3c6809…` | 177,4 | 1,4 ms |
| + sobreposição CPU/GPU entre frames | `cc3c6809…` (`rd_vk_frameoverlap 1`) | 200,8 | 0,5 ms |
| + fim das gravações de `phase.log` por frame | `ff810453…` | 361,7 / 367,5 | 0,6 ms |

Detalhes de cada otimização em `docs/VULKAN.md` §3.

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
