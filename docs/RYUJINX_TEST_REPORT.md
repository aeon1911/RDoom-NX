# Relatório de testes no Ryujinx — RDoom NX

Data: 2026-10-11. **Estes testes não substituem o teste em Switch físico,
que continua pendente.**

## Ambiente

| Item | Valor |
|---|---|
| Emulador | Ryujinx 1.3.3 (Ryubing, pacote `ryujinx 1.3.3-2`), backend gráfico Vulkan |
| Host | CachyOS, Intel i5-9600KF, AMD Radeon RX 9070 XT (RADV), 31 GB, .NET 10.0.4 |
| Dados do emulador | isolados em `test-env/ryujinx` (`--root-data-dir`); as chaves do usuário são só ligadas por symlink; `~/.config/Ryujinx` não é tocado |
| SD de teste | `test-env/ryujinx/sdcard/switch/rdoom`, instalado a partir de `dist/RDoom-NX` pelo próprio script |
| Conteúdo | Freedoom 0.13.0 (livre); DOOM II, Brutal Doom, Lexicon, Compendium e WadFusion do usuário por symlink (`test-env/local-content.txt`), nunca copiados para `dist/` |
| Controle | virtual, por `/dev/uinput` (`scripts/vpad.py`), com a identidade de um controle Xbox One; a entrada percorre evdev → SDL3 no Ryujinx → HID emulado → libnx/SDL2 no RDoom |
| Capturas | só a janela do Ryujinx que roda o RDoom (`import -window <id>`, por título); nunca a área de trabalho |

Cada execução (`scripts/ryujinx-test.sh <rótulo> <roteiro> <segundos>`)
guarda em `evidence/ryujinx/<rótulo>/`: sha256 do NRO, logs do launcher e do
motor, relatório de sessão, log do Ryujinx, registro das entradas, perfis
depois da execução e capturas.

## Resultados

### Vulkan (padrão)

| Teste | Resultado | Evidência |
|---|---|---|
| `vktest.nro` (fumaça) | 600 frames apresentados | `evidence/vktest/` |
| `vkmatrix.nro` (matriz de recursos) | atributo de vértice, UBO, SSBO, textura limpa e **imagem linear + blit** corretos; texturas enviadas por `vkCmdCopyBufferToImage` (compute no NVK) saem pretas com `texture()` e `texelFetch()`; `unpackUnorm4x8` (PRMT) dá branco em vez de magenta | `evidence/vkmatrix/03/` |
| FreeDoom 2: launcher → título → menu → MAP01, andar, mirar, atirar | ✅ | `33-vk-play/` |
| FreeDoom 2 direto no MAP01, frame overlap ligado | ✅ imagem correta | `36-vk-overlap/` |
| Brutal Doom v22 test 6 (DOOM II), MAP01, tiro, chute, movimento | ✅ 7200 frames, sem erro de script | `34-vk-brutal/` |
| Lexicon 1.0: título, menu, hub | ✅ | `35-vk-lexicon/` |
| Compendium v1a: HUBMAP (seletor) | ✅ | `35-vk-compendium/` |
| Relatório de sessão completo (pedido/ativo, instância, superfície, dispositivo, swapchain, frames) | ✅ em todos os acima | `engine-session.txt` de cada pasta |

### Build só com Vulkan (1.0.0)

O RDoom renderiza só com Vulkan: o backend OpenGL/GLES do UZDoom não é
compilado (`HAVE_OPENGL=OFF`, `HAVE_GLES2=OFF`). FreeDoom 2, novo jogo, MAP01,
tiros: ✅ 60 fps, `renderer_active: vulkan` (`90-vk-only-sanity/`).

### WadFusion

O motor recusa o `doom_fusion.ipk3` do usuário (108 erros de ZScript): ele
foi gerado para o UZDoom 4.14. O UZDoom 5.0.3 oficial para Linux dá os mesmos
erros. O launcher marca o perfil como Erro e explica. Ver
`MOD_COMPATIBILITY.md`.

### Referência: GZDoom-NX-Modern (Thorhax) Update-2

No mesmo Ryujinx, num diretório isolado (`test-env/ryujinx-thorhax`), o log
para em `[Vulkan] Creating Vulkan instance...`
(`evidence/reference-gzdoom-nx-modern/01-vulkan/gzdoom.log`). O RDoom passa
desse ponto por causa do bridge de TLS e da compatibilidade de `ALLOC_SPACE`
(`VULKAN.md` §2).

## Limitações do emulador observadas

- `PrmtI is not implemented` no tradutor de shaders do Ryujinx (afeta
  `unpackUnorm4x8` do NAK).
- Shaders de compute internos do NVK (cópias) não funcionam no Ryujinx —
  contornado no RDoom (uploads por blit, buffers escritos direto).
- `ALLOC_SPACE` com endereço fixo responde o início do bloco livre —
  contornado (`nx_nvcompat.c`, só ativo quando a peculiaridade existe).
- GCS/CHKFEAT do libgcc do GCC 15 não traduzidos — contornado no ELF.
- Picos de centenas de ms no primeiro uso de um shader (compilação no host).
- Endereços de crash (PC/LR) imprecisos; o RDoom grava migalhas em
  `logs/boot.log`.

## Desempenho

Ver `PERFORMANCE.md`: sem limite de FPS, 367,5 fps (Freedoom 2) e 291,6 fps
(Brutal Doom); no padrão, 60,0 fps e desvio < 1 ms.
