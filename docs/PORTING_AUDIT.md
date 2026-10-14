# Auditoria do port — RDoom NX

O que foi tomado de onde, o que mudou em relação ao UZDoom 5.0.3, e como
verificar cada afirmação. Data: 2026-10-11.

## 1. Bases

| Item | Valor | Verificação |
|---|---|---|
| Motor | UZDoom **5.0.3** (última estável), tag commit `7910df7102033fd7754a317f3254f24101135926` | `git -C engine log 7910df71 -1` |
| Port de Switch anterior (do usuário) | `uz5.0.3-nx3.7` … `uz5.0.3-nx4.13` (tela de carregamento, crash handler; o renderizador OpenGL/GLES não é compilado) | commits `cfe330446`…`a5b622b9c` |
| Trabalho de Vulkan em andamento (não commitado no port anterior) | importado como está | commit `fd9999c71` |
| Commits do RDoom | `d65b6d187` … `b28af9162` | `git -C engine log --oneline 7910df71..HEAD`, série em `patches/` |
| Toolchain | devkitPro `devkita64` digest `sha256:1fc388c3…0282`, GCC 15.2.0, libnx 4.12.0 | `docker/Dockerfile` |
| Mesa | 26.2.4 Horizon SDK, mesa-switch `606441c9`, sha256 `937cbda8…a392` | conferido pelo Dockerfile no build |
| Freedoom | 0.13.0, zip sha256 `3f9b264f…` conferido contra o CHECKSUM assinado | `third_party/freedoom/` |

Nenhum arquivo do usuário fora deste projeto foi alterado: as pastas
originais (`<pasta local do autor>`, `<pasta local do autor>`,
`DOOM MODs DataBase`, `~/.config/Ryujinx`) só foram lidas; o Ryujinx de teste
usa um diretório de dados isolado (`test-env/ryujinx`, `--root-data-dir`).

## 2. Mudanças no motor (resumo por commit do RDoom)

| Commit | Conteúdo |
|---|---|
| `d65b6d187` | bridge de TLS (`TPIDR_EL0`) para o NAK; compatibilidade de `ALLOC_SPACE` fixo do Ryujinx |
| `3335b5daa` | launcher integrado; SD fixo em `sdmc:/switch/rdoom`; ligação com o SDK Mesa 26; relatório de sessão |
| `6616531c8` | log passo a passo do Vulkan; buffers em memória unificada; consultas de timestamp sob demanda; correção de `CompileNextShader`; glthread do Mesa 26 |
| `e4dada475` | caminhos `sdmc:/…` tratados como absolutos (IWAD não encontrada) |
| `bb137dee3` | sensibilidade, zona morta e inversão por perfil (`rd_joy_*`) |
| `f795ee1c7` | upload de texturas por imagem linear + blit (sem compute) |
| `b28af9162` | limpeza de cache só em memória cacheada; sobreposição CPU/GPU entre frames; fim do `phase.log` por frame; medições Vulkan no `perf.log` |

Outras alterações de build: `scripts/patch_chkfeat.py` (instruções GCS do
libgcc), NACP "RDoom", ícone e NRO gerados no pós-build do CMake;
`tools/nxlauncher` removido (substituído pelo launcher em `launcher/`).

Simulação: **ticrate de 35 Hz inalterado**. Nenhuma mudança de jogabilidade
em relação ao UZDoom 5.0.3 (o commit `0284cce00` do port anterior restaurou
`attacks.zs` byte a byte).

## 3. Política de clocks (sem overclock)

- O RDoom não chama `pcv*`, `clkrst*`, `apmSetPerformanceConfiguration` nem
  `appletSetCpuBoostMode`, não tem opção de frequência e não depende de
  sys-clk ou similares.
- `tests/check_no_overclock.sh` (parte de `scripts/test.sh`) verifica o ELF
  final e os fontes:
  - `pcv*`/`clkrst*` (controle de clock e tensão) não estão ligados;
  - `apmInitialize`, `apmSetPerformanceConfiguration` e
    `appletSetCpuBoostMode` aparecem no ELF porque o `applet.c` do próprio
    libnx os referencia em todo homebrew. O teste exige que **todas** as
    chamadas estejam em `_appletInitialize`/`_appletCleanup` do libnx
    (3 locais, conferidos por `addr2line`) e que
    `__nx_applet_PerformanceConfiguration` seja o padrão zerado do libnx
    (`.bss`). Com isso o libnx nunca chama `apmSetPerformanceConfiguration`, e
    `appletSetCpuBoostMode` só seria usado para voltar ao modo Normal;
  - nenhum fonte, script ou configuração do RDoom chama essas APIs nem cita
    ferramentas de overclock.
- O "estabilizador de FPS" herdado (`nx_stabilizer`, desligado por padrão)
  só reduz detalhes (luzes, distância de efeitos); não mexe em clocks.

## 4. Conteúdo e licenças

- Nada comercial é distribuído. `tests/check_package.py` recusa qualquer IWAD
  no pacote além de `freedoom1.wad`/`freedoom2.wad` (só no zip "-Freedoom").
- O `DOOM2.WAD` usado nos testes é do usuário, ligado por symlink no SD de
  teste; nunca entra em `dist/`.
- Licenças: `THIRD_PARTY_NOTICES.md` e `switch/rdoom/licenses/` no pacote.
- Arte: vetorial/procedural (`assets/src/generate_art.py`). Não houve
  ferramenta de geração de imagens por IA neste ambiente; os prompts estão
  em `assets/prompts/` para uma futura versão gerada, se desejado.

## 5. Estado de verificação

| Ambiente | Situação |
|---|---|
| Build reproduzível (Docker) | ✅ |
| Testes de host (launcher, UI, sem overclock, pacote) | ✅ `scripts/test.sh` |
| Ryujinx 1.3.3 com o NRO empacotado | ✅ Vulkan; ver `RYUJINX_TEST_REPORT.md` |
| **Switch físico** (Lite, portátil) | ✅ Freedoom 2, Brutal Doom, Lexicon (+ slaughter), Compendium, Ashes 2063, Aliens Eradication TC, Trailblazer; ver `STATUS.md` |

## 6. Problemas em aberto

1. WadFusion: o arquivo do usuário foi gerado para o UZDoom 4.14; precisa ser
   regerado com o WadFusion v1.6.1 (ver `MOD_COMPATIBILITY.md`).
2. Ryujinx não implementa `PRMT` imediato (afeta só o emulador).
