# Vulkan no RDoom NX

O Vulkan é o único renderizador do RDoom. Ele roda pelo **Mesa NVK** (driver
Vulkan aberto para GPUs NVIDIA) portado para o Horizon, ligado estaticamente
ao `RDoom.nro`, sem loader e sem nenhuma dependência instalada no console.
O backend OpenGL/GLES do UZDoom não é compilado (`HAVE_OPENGL=OFF`,
`HAVE_GLES2=OFF`). Se o Vulkan falhar, o jogo para com uma mensagem
(pt-BR/en) dizendo o que falhou e onde está o relatório.

Estado em 2026-10-14:

| Ambiente | Situação |
|---|---|
| Build (Docker, devkitA64 + Mesa 26.2.4 Horizon SDK) | ✅ reproduzível, NRO com o backend Vulkan incluído |
| Ryujinx 1.3.3 (Ryubing) | ✅ instância, superfície VI, dispositivo NVK GM20B, swapchain, dezenas de milhares de frames apresentados; FreeDoom 2, Brutal Doom, Lexicon e Compendium jogáveis com imagem correta |
| Switch físico (Lite, portátil) | ✅ Freedoom 2, Brutal Doom, Lexicon (+ slaughter), Compendium, Ashes 2063, Aliens Eradication TC e Trailblazer, 45–60 fps; ver `docs/STATUS.md` |

## 1. Componentes

| Peça | Origem |
|---|---|
| `libvulkan.a` (NVK + compilador NAK em Rust); `libEGL/libglapi` só porque o SDL2 depende delas | Mesa 26.2.4, danfromtico/mesa-switch `606441c9`, artefato de CI 11484346771 (cópia no release `deps-mesa-26.2.4-606441c9` deste repositório), sha256 `937cbda82d411c3ca909aa977b3ced82d4e6070046ab016911b1ce84e58ca392` (verificado no `docker/Dockerfile`) |
| Superfície | `VK_NN_vi_surface` sobre o `NWindow` padrão do libnx |
| Backend do motor | `engine/src/common/rendering/vulkan/` (UZDoom 5.0.3) + ZVulkan/VMA em `engine/libraries/ZVulkan` |
| Criação do contexto | `engine/src/common/platform/posix/sdl/sdlglvideo.cpp` |

## 2. O que precisou ser corrigido para o Vulkan funcionar

Cada item abaixo foi um bloqueio real, encontrado com logs e testes, não uma
suposição. As correções estão na série de patches de `engine/` (branch
`rdoom-nx`) e em `docker/`/`scripts/`.

1. **Índice de símbolos incompleto nos `.a` do SDK** (gerados com `llvm-ar` no
   MSYS2): o linker GNU não encontrava objetos. O Dockerfile roda
   `aarch64-none-elf-ranlib` em todas as bibliotecas do SDK.
2. **TLS do Rust (NAK)**: o código Rust lê o ponteiro de thread em
   `TPIDR_EL0`, que o libnx nunca escreve. `nx_tls_bridge.c` copia
   `__aarch64_read_tp()` para `TPIDR_EL0` na thread principal e em toda thread
   criada (`--wrap=threadCreate`). Sem isso o compilador de shaders travava.
3. **Reserva de VA fixa no Ryujinx**: o `nvhost-as ALLOC_SPACE` com endereço
   fixo do Ryujinx 1.3.3 devolve o início do bloco livre, não o endereço
   pedido. `nx_nvcompat.c` detecta a peculiaridade com uma sonda e, só quando
   ela existe, reserva a região de páginas grandes de uma vez e responde as
   reservas fixas localmente. Em hardware real a sonda dá resultado normal e
   nada muda (`nv_compat: inactive` no relatório).
4. **Instruções GCS/CHKFEAT do libgcc do GCC 15** que o Ryujinx não traduz:
   `scripts/patch_chkfeat.py` troca-as por equivalentes neutros no ELF
   (CHKFEAT → NOP, `mrs gcspr_el0` → `mov xN, xzr`, `gcspopm` → NOP). Em
   hardware (sem GCS) o comportamento é idêntico.
5. **Cópias por compute**: o NVK expõe uma única família de filas (com bit de
   compute) e executa `vkCmdCopyBuffer`/`FillBuffer`/`CopyBufferToImage` como
   shaders de compute internos. O Ryujinx não emula esses shaders e a cópia
   saía vazia/preta. No Switch a memória é unificada, então:
   - buffers estáticos e de streaming são escritos direto na memória visível
     pela CPU (`vk_hwbuffer.cpp`);
   - texturas sobem para uma imagem linear e são copiadas para a imagem final
     com `vkCmdBlitImage`, que roda no motor 3D (`vk_nxupload.cpp`,
     `rd_vk_blitupload`, padrão ligado). Também tira trabalho de compute da GPU
     em hardware real.
6. **Consultas de timestamp**: o NVK zera query pools com um dispatch de
   compute; o pool só é criado quando `stat gpu` é usado.
7. **Caminhos `sdmc:/`** e **re-entrada em `CompileNextShader`** (índice -1,
   "Unable to load garbage"): corrigidos no motor.

## 3. Otimizações para o Tegra X1

Medidas no Ryujinx (ver `docs/PERFORMANCE.md`). Cada uma tem prova antes/depois.

### 3.1 Limpeza de cache só onde a memória é cacheada

O Tegra X1 não faz snoop do cache de dados da CPU. O port herdado limpava
(`armDCacheClean`) **todos os buffers persistentes inteiros a cada submit**,
além de cada `Unmap` e cada bloco do buffer de luzes. O relatório de sessão
agora lista os tipos de memória do NVK:

```
vulkan_memory_types: 0:LVCa, 1:LVCo   (L=device local, V=host visible, Co=coherent, Ca=cached)
```

A desmontagem do SDK mostra que a memória sem cache é mapeada com
`svcSetMemoryAttribute(..., Uncached)` e que o próprio NVK pula a sincronização
para memória coerente. Os buffers persistentes do motor exigem
`HOST_COHERENT`, portanto ficam no tipo 1 (sem cache) e a limpeza era trabalho
puro (uma instrução `DC CVAC` por linha de 64 bytes de dezenas de MB).
Agora cada `VulkanBuffer`/`VulkanImage` sabe se a sua memória é `HOST_CACHED`
e só nesse caso limpa. Resultado: custo de fim de frame 5,7 → 0,5 ms/frame;
FreeDoom MAP01 97 → 177 fps no Ryujinx.

### 3.2 Sobreposição CPU/GPU entre frames (`rd_vk_frameoverlap`, padrão 1)

O UZDoom esperava a GPU terminar o frame dentro do próprio `Update()`. Agora
o fim do frame faz submit e present e **não espera**; a espera acontece no
início do frame seguinte (`BeginFrame`/`Update`), antes de qualquer outro
submit e no carregamento de fase — exatamente nos pontos em que os buffers do
frame anterior seriam reescritos. Command buffers, staging e objetos apagados
do frame pendente ficam vivos até a espera. Com isso a GPU desenha o frame N
enquanto a CPU roda os tics, a entrada e o som do frame N+1.
`stat gpu` (timestamps) desliga a sobreposição enquanto está ativo.
FreeDoom MAP01: 177 → 201 fps (antes da correção do `phase.log`).

### 3.3 Escrita de log por frame removida

`NX_Phase` (diagnóstico herdado) abria, escrevia e fechava `phase.log` no
cartão SD **cinco vezes por frame**; o arquivo chegou a 15 MB num teste. Agora
cada fase é registrada uma vez (até 200 linhas); o relatório de travamento
continua sabendo a fase atual pela "migalha" em memória: 201 → 362–368 fps
(FreeDoom MAP01, Ryujinx).

### Resultado (Ryujinx, sem limite de FPS, mesma build)

| Cena | Vulkan |
|---|---|
| FreeDoom 2 MAP01 | **367,5 fps** (2,72 ms) |
| Brutal Doom v22 test 6, DOOM II MAP01 | **291,6 fps** (3,43 ms) |

No padrão (vsync, 60 FPS) o Vulkan fica em 60,0 fps com desvio de
0,69–0,85 ms e nenhum frame acima de 25 ms nas duas cenas.

Números de emulador mostram custo relativo de CPU/driver; não são os FPS do
console. Ver `docs/PERFORMANCE.md`.

## 4. Critérios de "Vulkan funcionando" e onde está a prova

O motor escreve `logs/engine-session.txt` passo a passo
(`RDoom_SessionSet`). Uma sessão Vulkan bem-sucedida tem todas as linhas:

```
renderer_requested: vulkan
nv_compat: ACTIVE: fixed ALLOC_SPACE ... (emulator quirk)      # "inactive" em hardware
vulkan_instance: ok (api 1.2, loaderless Mesa NVK)
vulkan_surface: VK_NN_vi_surface on the default NWindow: ok
vulkan_device: NVIDIA Tegra X1 (GM20B) (NVK GM20B) (api 1.4.354, driver 26.2.4)
renderer_active: vulkan
vulkan_memory_types: 0:LVCa, 1:LVCo ...
vulkan_frame_overlap: on
vulkan_swapchain: 1280x720, 3 images, format 44, vsync on (FIFO)
vulkan_frames_presented: 6600
```

| Critério | Prova |
|---|---|
| Backend incluído no NRO | símbolos `vk*`/NVK em `build/nx/uzdoom.elf`; `renderer_active: vulkan` |
| Instância, superfície, dispositivo | linhas acima, `evidence/ryujinx/32-vk-freedoom/engine-session.txt` |
| Frames apresentados | `vulkan_frames_presented` (1º frame e a cada 600) |
| Mapa renderizado | `evidence/ryujinx/32-vk-freedoom/01-map.png`, `33-vk-play/*.png`, `34-vk-brutal/*.png`, `35-vk-lexicon/*.png`, `35-vk-compendium/*.png`, `36-vk-overlap/*.png` |
| Encerramento limpo | `engine.log`/`rdoom.log` terminam sem crash (`logs/crash.log` ausente) |
| Pedido x ativo distinguidos | `renderer_requested` e `renderer_active` são linhas separadas; falha grava `vulkan_error` e `renderer_active: none` |

## 5. Comparação com GZDoom-NX-Modern (Thorhax)

O GZDoom-NX-Modern Update-2 (o port que motivou o uso do Vulkan) foi executado
no **mesmo** Ryujinx 1.3.3, num diretório isolado
(`test-env/ryujinx-thorhax`). O log dele para em
`[Vulkan] Creating Vulkan instance...` e o jogo não passa disso
(`evidence/reference-gzdoom-nx-modern/01-vulkan/gzdoom.log`). As correções 2–5
acima explicam por quê: sem o bridge de TLS e sem a compatibilidade de
`ALLOC_SPACE`, o NVK não inicializa nesse emulador. Uma comparação de
desempenho direta só será possível no Switch físico.

## 6. Limitações conhecidas

- **Ryujinx**: o tradutor de shaders não implementa `PRMT` com imediato
  (`PrmtI is not implemented`), instrução que o NAK emite para
  `unpackUnorm4x8`. Afeta só cores empacotadas em alguns efeitos no emulador.
- **Switch físico (primeiro teste, 2026-10-11)**: `vulkan_memory_types` saiu
  `0:LVCa, 1:LVCo` e `nv_compat: inactive`, como previsto, e o jogo rodou fluido;
  mas houve `device lost` (ver `STATUS.md`). Proteções adicionadas: sem gravar o
  cache de pipelines após perda da GPU, limpeza automática dos caches na sessão
  seguinte, ação "Limpar caches de shaders" no Diagnóstico, `rdoom_debug.txt`
  (variáveis do driver em MAIÚSCULAS, como `NVK_DEBUG=push_sync`; CVARs em
  minúsculas, como `rd_vk_frameoverlap=0`) e a linha `vulkan_last_new_pipeline`
  no relatório de sessão.
- Swapchain: o UZDoom pede 2 imagens com vsync, mas o NVK no VI exige no
  mínimo 3, então a apresentação já é em buffer triplo (FIFO). Com frames
  acima de 16,7 ms isso dá taxa intermediária (ex.: 45 fps) em vez de cair
  direto para 30.
- Lightmaps (mapas com `LIGHTMAP` do UZDoom) ainda sobem por
  `vkCmdCopyBufferToImage` (compute no NVK); nenhum mod testado usa lightmaps.

## 7. CVars e diagnóstico

| CVar | Padrão | Efeito |
|---|---|---|
| `vid_preferbackend` | 1 (Vulkan) | o motor força 1 no Switch (configs antigas com 0/2 viram Vulkan) |
| `rd_vk_frameoverlap` | 1 | sobreposição CPU/GPU entre frames (§3.2) |
| `rd_vk_blitupload` | 1 | upload de texturas por imagem linear + blit (§2.5) |
| `nx_perflog` | 1 | `perf.log` com resumos a cada 10 s (2 = todo frame) |

`perf.log` traz, no Vulkan, a linha:

```
#   vulkan: submits 1.0 (0.10 ms in vkQueueSubmit) | cache clean 0.00 | acquire 0.08 | present 0.17 | fence wait at frame end 0.00 | wait for previous frame at frame start 0.10
```

Build de diagnóstico com rastreamento de cada `vkCmd*`/`vkQueueSubmit` em
`logs/boot.log`: `-DRDOOM_VK_TRACE=ON` (o pacote é gerado com `OFF`).
