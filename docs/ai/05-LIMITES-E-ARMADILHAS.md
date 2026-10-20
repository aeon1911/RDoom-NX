# 05 — Limites conhecidos, becos sem saída e armadilhas

Leia antes de propor "otimizações": muitas ideias óbvias já foram testadas e
medidas.

## Limites que ficam (sem solução no port)

| Situação | Medida | Por quê |
|---|---|---|
| Project Brutality em lutas com explosões e muito gore | 36–50 fps | CPU: lógica do PB 8–12 ms por quadro (~400 manchas de sangue, ~140 gibs), `A_Explode`/gibs 13–22 ms por ação. GPU folgada (10–13 ms). Mitigação do jogador: reduzir gore nas opções do PB |
| Brutal Doom em mapa slaughter gigante (7500 objetos, DKTS01 Arch Bridge) | 12–14 fps | CPU: tic de 44–56 ms (800 imps do Brutal = 13,8 ms, ~1200 alvos de headshot, colisão 6,7 ms). O mesmo mapa sem Brutal: 56–60 fps |
| Brutal Doom + Lexicon slaughter (~4000 objetos) | 43–54 fps | CPU (scripts do Brutal) |
| Hub do Lexicon | ~33 fps | GPU; o dono pediu para não mexer |
| Carregamento do Project Brutality | ~11–12 s de scripts, ~600 MB lidos do SD | tamanho do mod |
| Visor do capacete do PB | ~3 ms de GPU | desligar no menu do PB dá resolução maior |
| Lightmaps do UZDoom | sobem por `CopyBufferToImage` (compute no NVK) | nenhum mod testado usa |
| Modo dock, sessões de horas | fora de escopo | Switch Lite; mapas não duram horas |

Com o Switch em clocks de fábrica, o limite do PB é a CPU A57 a ~1 GHz. Não há
resolução, shader ou ajuste de GPU que resolva essas cenas.

## Becos sem saída já explorados (não repetir sem motivo novo)

- **"device lost"**: ZCULL, caches no SD, sobreposição de frames, compressão,
  `NO_UBO_CBUF`, `MAPPED_COMPLETION`, profundidade D32S8 e versões do Mesa
  (26.2.2/3/4) **não eram a causa**. A causa era o cabeçalho de luzes (P09).
- **Limpeza de cache da CPU**: a memória coerente do NVK é sem cache no
  console; remover as limpezas foi correto.
- **Mixagem de áudio roubando CPU**: o SDL2 do devkitPro dá prioridade `0x3B`
  (a mais baixa) à thread de áudio; ela não interrompe o jogo.
- **Corte de efeitos colados na câmera** (`rd_fx_nearcull`): não resolve a
  queda com fumaça do PB, porque o limite é CPU (ficou ligado, é barato).
- **`vkCmdBlitImage` para ampliar a cena**: 2–4 ms no NVK; trocado por desenho.
- **Comparações com OpenGL/GLES**: removidos; não reintroduzir.
- **Copiar soluções do GZDoom-NX-Modern**: proibido pelo dono.
- **Overclock / boost de CPU**: proibido.
- **Aumentar o ticrate**: proibido.

## Armadilhas do Ryujinx

- **Timestamps de GPU leem 0**: a resolução dinâmica pela GPU fica inativa;
  fps do emulador não dizem nada sobre o custo de GPU no console.
- **Não reproduz falhas de GPU** (device lost) nem memória com lixo.
- **Visões de profundidade do NVK não são amostradas** (`Invalid texture format
  0x2512B`): a camada de meia resolução se desliga sozinha. Para inspecioná-la,
  use `rd_halfeffects_debug` 4/5/10.
- **Depois de algumas centenas de texturas**, paredes, pisos e HUD ficam
  pretos; às vezes a vista inteira fica preta ou sem arma. É o cache de
  texturas do emulador, independente das threads. Para validar visual:
  `rd_sprite_prefetch 0`, `gl_precache 0` e lista de texturas vazia.
- **Brutal Doom pelo caminho do título** fica com tela preta (pelo `warp`
  funciona).
- **Primeiro tiro custa 40–60 ms** em ações nativas variadas: tradução JIT do
  emulador, não é real.
- **Primeiro uso de um shader** dá picos de centenas de ms (compilação no host).
- Endereços de crash (PC/LR) imprecisos: use as migalhas de `logs/boot.log`.
- `PrmtI is not implemented`: cores empacotadas erradas em alguns efeitos, só
  no emulador.
- O pipeline de criação custa 1,6 ms no Ryujinx contra ~20 ms no console; a
  destruição de imagens é barata no emulador e cara no console (~0,23 ms cada).

## Armadilhas do console e do SD

- **Qualquer gravação no SD durante o jogo causa engasgo.** Logs vão para a
  memória e uma thread no núcleo 2 grava; listas são gravadas raramente.
- O dono às vezes copia só o `RDoom.nro`: um lump novo no `uzdoom.pk3` precisa
  ser opcional ou verificado (P18).
- Atualizar copiando a pasta inteira apaga `cache/` (pipelines e texturas); a
  semente no pacote cobre os pipelines.
- O `.ini` do perfil guarda valores antigos de CVARs arquivadas; para mudar um
  padrão use a migração `rd_defaults_rev`.
- No Switch a GPU e o `malloc` dividem ~3,2 GB; o heap do PB fica em
  2,2–2,4 GB. Recursos de GPU devem ser liberados aos poucos (P30).
- Threads criadas sem núcleo explícito caem no núcleo 0, o do jogo. Workers
  ficam nos núcleos 1/2.
- `extern FBoolCVar` escrito à mão derruba o jogo: use `EXTERN_CVAR`.
- `thread_local` dentro do `malloc` interceptado trava a inicialização.

## Ideias que nunca foram feitas (se alguém quiser continuar)

- Liberação de texturas: mover a destruição das imagens para um worker; a
  varredura (5–9 ms a cada 30 s com o PB) poderia ser incremental.
- Lightmaps por blit em vez de compute.
- Medir no console o tempo que a thread do jogo perde para outras threads
  (`svcGetInfo` com `InfoType_ThreadTickCount`).
- Juntar à semente de pipelines (`scripts/merge-pipelines.py`) os pipelines de
  mods novos testados.
