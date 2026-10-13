# RDoom NX

Port não oficial do **UZDoom 5.0.3** para Nintendo Switch desbloqueado
(Atmosphère, homebrew `.nro`), com **Vulkan como renderizador principal**
(Mesa NVK), OpenGL/OpenGL ES como opções explícitas e um launcher próprio,
o **RDoom NX**, com bibliotecas de IWADs e mods, ordem de carregamento e
perfis.

**Somente os clocks de fábrica.** O RDoom não tem código de overclock, nem
opção de frequência, nem dependência de sys-clk (verificado por
`tests/check_no_overclock.sh`).

## Estado (2026-10-13)

| | |
|---|---|
| Build reproduzível | ✅ `scripts/build.sh` (Docker) |
| **Switch físico** (Lite, portátil) | ✅ Vulkan estável: Freedoom 2, DOOM II, Brutal Doom 21, Trailblazer. 60 fps na maior parte do tempo; picos de fumaça do Brutal Doom contidos pelo estabilizador e pela resolução dinâmica |
| Ryujinx 1.3.x | ✅ Vulkan, OpenGL e GLES (com as ressalvas de `docs/RYUJINX_TEST_REPORT.md`) |
| Desempenho | ver `docs/PERFORMANCE.md` e o histórico de testes em `docs/STATUS.md` |

## Instalação

1. Copie `switch/` do zip `RDoom-NX-1.0.0.zip` (ou `…-Freedoom.zip`, que já
   traz as IWADs livres do Freedoom) para a raiz do cartão SD.
2. IWADs em `sdmc:/switch/rdoom/iwads/`, mods em `sdmc:/switch/rdoom/mods/`.
3. Abra o RDoom pelo Homebrew Menu **em modo título** (segure R ao abrir um
   jogo). Escolha o perfil com L/R e aperte **+**.

Detalhes em `docs/INSTALL-pt-BR.txt` (também dentro do pacote).

## Perfis prontos

FreeDoom 2 — Vulkan Stock · Brutal Doom · WadFusion · Lexicon · Compendium ·
Configuração personalizada. Todos com a configuração **"RDoom — Stock
Switch"** (Vulkan, 1280×720, vsync, 60 FPS, sem pós-processamento pesado).
Criar, duplicar, renomear, editar, apagar, exportar, importar e restaurar pelo
launcher. Formato e regras em `docs/PROFILES.md`.

## Compilar

Requisitos no host: Docker, git, python3, rsvg-convert, ImageMagick.

```bash
scripts/build.sh
```

Clone com `git clone --recursive` (o motor é um submódulo). Gera `dist/RDoom.nro`, `dist/RDoom-NX/` (árvore do SD), os zips e
`dist/SHA256SUMS.txt`. Incremental: `scripts/dev-rebuild.sh`.
Testes: `scripts/test.sh` (host) e `scripts/ryujinx-test.sh`,
`scripts/bench-ryujinx.sh`, `scripts/pacing-ryujinx.sh` (emulador).

## Estrutura

| Pasta | Conteúdo |
|---|---|
| `engine/` | UZDoom 5.0.3 + patches do port (submódulo: [RDoom-NX-engine](https://github.com/aeon1911/RDoom-NX-engine), branch `rdoom-nx`) |
| `patches/` | a mesma série exportada (`git format-patch`) |
| `launcher/` | launcher RDoom NX (C++17) |
| `docker/` | imagem de build (devkitA64, libnx, Mesa SDK verificado) |
| `scripts/` | build, empacotamento, testes no emulador, benchmarks |
| `tests/` | testes de host, verificação de pacote e de overclock |
| `vktest/` | NROs de teste de Vulkan (fumaça e matriz de recursos) |
| `assets/` | arte do launcher (gerada por script) |
| `docs/` | documentação |

## Documentação

- `docs/VULKAN.md` — como o Vulkan funciona aqui, o que foi corrigido, otimizações, critérios e provas
- `docs/PERFORMANCE.md` — benchmarks e ritmo de frames, método e ressalvas
- `docs/ARCHITECTURE.md` — fluxo de boot, módulos, layout do SD, logs
- `docs/PROFILES.md` — perfis e modelos
- `docs/MOD_COMPATIBILITY.md` — mods testados (e o caso do WadFusion)
- `docs/RYUJINX_TEST_REPORT.md` — ambiente e resultados no emulador
- `docs/PORTING_AUDIT.md` — origem de cada parte, mudanças, política de clocks, pendências
- `THIRD_PARTY_NOTICES.md` — licenças

## Licença

GPL-3.0 (a do UZDoom). Componentes de terceiros em `THIRD_PARTY_NOTICES.md`.
"Doom" é marca dos seus donos; o RDoom não inclui dados comerciais (IWADs
comerciais e mods não são distribuídos: use os seus próprios arquivos ou o
Freedoom).
