# 04 — Fluxos de trabalho

Como compilar, testar, analisar logs do console e publicar. Todos os comandos
partem da raiz do projeto (`/mnt/steam/Projetos/RDooom` na máquina do dono;
shell padrão: fish).

## 1. Build

| Comando | O que faz | Tempo |
|---|---|---|
| `scripts/build.sh` | imagem Docker `rdoom-nx-build` (se faltar), Mesa SDK (release espelho, conferido por SHA-256), Freedoom, arte, motor + launcher, NRO, pacote `dist/` e `scripts/test.sh` | ~10 min com cache |
| `scripts/dev-rebuild.sh` | só `ninja -C build/nx` dentro do Docker + `scripts/package.sh` | 1–3 min |
| `scripts/package.sh` | `dist/RDoom-NX/` (árvore do SD), `RDoom-NX-<versão>.zip`, `…-Freedoom.zip`, `SHA256SUMS.txt` | segundos |

- Saída: `dist/RDoom.nro`, os zips e `dist/SHA256SUMS.txt`. Logs de build em
  `build/logs/` (`nx-dev.log` no incremental).
- Versão: `set( RDOOM_VERSION "x.y.z" )` em `engine/CMakeLists.txt` e
  `info.version` em `tests/ui_preview.cpp`.
- O pós-build do CMake gera o NACP, remove símbolos, aplica `patch_chkfeat.py`
  e gera o NRO. O build confere que nenhum objeto do RDoom lê `TPIDR_EL0`.
- Ao mudar o `docker/Dockerfile`, a imagem é refeita pelo `build.sh`.

## 2. Testes de host

`scripts/test.sh` executa quatro passos:
1. testes unitários do launcher (246 verificações; ASan/UBSan);
2. smoke de UI (`ui_preview`, 13 telas, plano de inicialização);
3. `tests/check_no_overclock.sh` no ELF;
4. `tests/check_package.py` no `dist/` (recusa IWAD comercial, confere arquivos).

Precisa de `ninja` no host. Sem ele, configure com
`cmake -S tests -B build/host-tests-make -G "Unix Makefiles"` e rode os mesmos
binários (P44).

## 3. Testes no Ryujinx

**Antes de tudo:** `mount | grep -ci mtp` precisa dar 0. Com o SD do Switch
montado por MTP (DBI) o PC do dono travou.

```
PROFILE=<id> scripts/ryujinx-test.sh <nome-da-execução> <roteiro.txt|-> [segundos]
```

- Instala `dist/RDoom-NX` no SD emulado (`test-env/ryujinx/sdcard/switch/rdoom`)
  e liga IWADs e mods locais por symlink (`test-env/local-content.txt`).
- `PROFILE=` define o perfil ativo do launcher; `RESET=1` começa do zero.
- Guarda tudo em `evidence/ryujinx/<nome>/`: logs do motor e do launcher,
  `perf.log`, log do Ryujinx, entradas, capturas, sha256 do NRO.
- Roteiros do controle virtual (`scripts/vpad.py`, uinput): `wait`, `tap`,
  `hold`/`release`, `stick L|R x y [s]`, `mark`, `shot <nome>` (captura só a
  janela do Ryujinx). Os botões têm nomes do Switch (`A B X Y L R ZL ZR PLUS
  MINUS UP DOWN LEFT RIGHT`). Exemplos em `scripts/vpad/` e `build/vpad-*.txt`.
- Mudar o perfil de teste: `python3 scripts/test-profile.py <id> chave=valor`.
  Atenção: `'cvars={...}'` substitui o dicionário inteiro; volte a
  `'cvars={}'` depois.
- Ir direto a um mapa e automatizar: crie `autoexec.cfg` no SD emulado e apague
  depois. Exemplo usado nos testes de memória do PB (WadFusion + PB, armas em
  ciclo):

  ```
  alias rd_cyc "weapnext; wait 140; rd_cyc"
  wait 70; map lr_map08; wait 500; give all; wait 35; rd_cyc
  ```

  O warp do perfil só aceita `MAPxx`/`ExMy`.
- Conteúdo de teste próprio: `test-env/content/rdtest-horde.pk3` (150 imps,
  modo deus), `rdtest-smoke.pk3` (fumaça e fogo na frente do jogador).
- Benchmarks: `scripts/bench-ryujinx.sh <perfil> <rótulo> [mapa] [s]` (sem
  limite de fps) e `scripts/pacing-ryujinx.sh` (vsync 60, todos os frames).
  Conferem que a captura não está preta.

**O que o Ryujinx não mostra** (ver `05-LIMITES-E-ARMADILHAS.md`): timestamps
de GPU (lê 0), falhas e travamentos da GPU real, custo real de pipelines e de
destruição de imagens, a camada de meia resolução (desliga sozinha) e o visual
depois de algumas centenas de texturas (fica preto). Use-o para crash,
correção lógica, logs e custo relativo de CPU.

## 4. Analisar logs do console

O dono joga no Switch Lite e manda a pasta `logs/` do SD (atual +
`anterior/` … `anterior-5/`). Passos:

1. **Copiar primeiro** para `evidence/hw/<data-nome>/`, porque ele apaga do SD.
   Olhe o que há na pasta antes de copiar.
2. `engine.log`: versão (`UZDoom version 5.0.3.NN+hash`), arquivos carregados,
   mapas jogados, erros de script, "Game saved".
3. `last-launch.txt`: perfil e argv. `engine-session.txt`: passos do Vulkan,
   frames apresentados, warm-ups (`vulkan_texture_warmup`,
   `vulkan_pipeline_warmup`), camada de meia resolução.
4. `perf.log`:
   - `python3 -I scripts/perf-summary.py perf.log` dá uma linha por janela de
     5 s. A coluna `t` só avança quando houve frame lento (as linhas de frame
     só aparecem acima de `nx_perfthreshold`).
   - Frames lentos: `awk '!/^#/ && $2+0>250' perf.log`. Separe troca de mapa
     (tic de ~1000 ms + centenas de texturas), carregar save (tic de vários
     segundos), salvar o jogo (tic de 300–400 ms) e travamento real em jogo.
   - Notas: `grep "note" perf.log` (`texture eviction`, `slow action <classe>
     (arquivo:linha)`, `slow texture upload`).
   - Formato de cada linha de frame (cabeçalho na linha 3):
     `time frame | tics tic think | display: scene bsp hud update gpuwait swap |
     items transl | shaders n/ms | textures n/ms/kpix | sounds | sndupdate
     sndstart | io | async tex | draws | lights | stab | player spawn |
     startsound | begin pending = wait/save/buffers/sprites, slowest upload`.
     Um `pending` enorme indica espera ou destruição no início do quadro.
   - Bloco de resumo (`# summary`): fps, piores frames, divisão do tempo por
     frame, GPU por timestamps (`gpu (handheld): frame … = 3D scene + post +
     2D/present`), resolução, efeitos na camada, memória (`malloc in use`), sons
     (`sounds:`, `sound restarts:`) e custo das classes de ator
     (`thinkers by class`).
5. Regra de diagnóstico:
   - GPU (`gpuF`) perto de 16,7 ms → limite de GPU (fumaça, resolução);
   - GPU folgada com tic + cena grandes → CPU (scripts do mod);
   - `pending`/`begin` altos → algo no início do quadro (espera, destruição,
     gravação).
6. Compare com as sessões anteriores do mesmo conteúdo em `evidence/hw/`.

## 5. Publicar uma versão

1. Commit no motor (autor noreply, trailer de coautoria) e push:
   `git -C engine -c credential.helper= -c credential.helper='!gh auth git-credential' push rdoom rdoom-nx`
2. Regerar a série:
   `rm patches/0*.patch && git -C engine format-patch -q 7910df71..HEAD -o ../patches && (cd patches && sha256sum 0*.patch > SHA256SUMS.txt)`
3. `scripts/build.sh` (completo) e conferir os testes.
4. Atualizar `docs/STATUS.md` (cabeçalho com sha256 do NRO e versão do motor,
   seção do teste), `README.md` (seção Estado, nome do zip, tabela) e o que mais
   mudou.
5. `git fetch origin` e merge de `origin/main` se o dono editou algo no GitHub.
   Conferir `git diff --cached --name-only` (nada de wad/pk3/evidence/test-env).
   Commit no repositório principal com o submódulo atualizado; push de `main`.
6. Release:
   `gh release create vX.Y.Z dist/RDoom-NX-X.Y.Z.zip dist/RDoom-NX-X.Y.Z-Freedoom.zip dist/RDoom.nro dist/SHA256SUMS.txt --repo aeon1911/RDoom-NX --title "RDoom NX X.Y.Z" --notes-file <notas.md> --target main --latest`
   Notas em pt-BR: o que mudou, o que foi medido, instalação.
7. Avisar o dono do que copiar para o SD. Se só o `RDoom.nro` mudou, basta ele;
   se mudou o `uzdoom.pk3` ou outros arquivos do pacote, copiar o conjunto.
   Copiar a pasta inteira apaga `cache/`.

Mensagens de commit seguem o padrão do histórico, por exemplo
"RDoom NX 1.1.4: …" no repositório principal e "RDoom 1.1.4: …" no motor, com
corpo explicando sintoma, causa, correção e medição.

## 6. Comunicação com o dono

- Sempre em pt-BR, direto, com números medidos.
- Terminar cada entrega com "o que falta".
- Pedidos de teste no console devem dizer exatamente o que jogar e o que olhar.
- Ações destrutivas ou públicas (push forçado, apagar, publicar) só com pedido
  explícito.
