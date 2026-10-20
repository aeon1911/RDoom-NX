# 01 — História do projeto, do início ao fim

Linha do tempo do RDoom NX. Para cada fase: objetivo, o que foi feito e o
resultado medido. O detalhe teste a teste está em `docs/STATUS.md`; as causas
e correções de cada problema estão em `02-PROBLEMAS-E-SOLUCOES.md`.

Aviso sobre hashes: em 2026-10-13 o autor dos commits do motor foi trocado para
o e-mail noreply do GitHub, o que mudou todos os hashes do motor. Hashes
antigos (ex.: `b173c0a27`, `d65b6d187`, `af152e499`) aparecem em `STATUS.md`,
`PORTING_AUDIT.md` e em `evidence/`; os atuais estão em
`git -C engine log 7910df71..HEAD`. A série completa também está exportada em
`patches/` (66 patches na 1.1.4).

## Fase 0 — Base herdada (2026-09-28 a 09-30)

Antes do RDoom, o dono tinha um port próprio do UZDoom 5.0.3 para Switch
(`uz5.0.3-nx3.7` … `nx4.13`, em `/mnt/steam/UZDOOM SWITCH PORT/uzdoom-switch`)
com renderizador OpenGL/GLES, tela de carregamento, crash handler e um trabalho
de Vulkan não commitado. Esses commits viraram a base do motor (patches
0001–0012): ajustes de compilação (Abseil, TiMidity, ZMusic, libdl, strip),
remoção de comportamento exclusivo do Project Brutality, `attacks.zs`
restaurado byte a byte, opção "Estabilizar FPS".

## Fase 1 — Vulkan funcionando (2026-10-11)

Objetivo: renderizar com Mesa NVK no Switch, já que o GLES era lento.

- Importado o trabalho de Vulkan em andamento (patch 0013).
- O SDK do Mesa 26.2.4 para Horizon (danfromtico/mesa-switch, CI `606441c9`)
  precisou de cinco correções para sequer inicializar: índice dos `.a`
  (ranlib), TLS do Rust/NAK (`TPIDR_EL0`), reserva de VA fixa do Ryujinx,
  instruções GCS/CHKFEAT do GCC 15 e cópias por compute que o Ryujinx não
  executa (uploads por imagem linear + blit). Ver problemas P01–P05.
- Launcher próprio integrado ao NRO, SD fixo em `sdmc:/switch/rdoom`,
  relatório de sessão passo a passo (`logs/engine-session.txt`), controles por
  perfil (`rd_joy_*`).
- Otimizações iniciais (Ryujinx, Freedoom MAP01, sem limite): 97 → 367 fps.
  Limpeza de cache só em memória cacheada, sobreposição CPU/GPU entre frames e
  fim da gravação de `phase.log` a cada frame (P06–P08).

## Fase 2 — "device lost" no console (2026-10-11 a 10-12)

Primeiro teste no Switch: muito fluido, mas a GPU caía (`device lost`, depois
`channel 2 lost: fifo idle timeout`) ao atirar ou ao entrar no mapa.
Investigação em oito testes no console com uma "escada" automática de
contornos a cada perda da GPU, rastreamento por desenho, dump do push buffer e
um kit com três versões do Mesa. Causa real: o cabeçalho da lista de luzes
dinâmicas lido como lixo no hardware, gerando laços de shader praticamente
infinitos. Correção: validação do cabeçalho em `getLightRange()` (`main.fp`).
Vários suspeitos foram descartados no caminho (ZCULL, caches, sobreposição,
regressão do driver). Ver P09.

## Fase 3 — Fluidez no console (2026-10-12 a 10-14)

Com o jogo estável, o `perf.log` real mostrou o que pesava. Em ordem:

1. CPU e GPU em série → envio da cena antes de esperar a swapchain, dois frames
   em voo, cena RGBA8, fim da cópia da cena (P10).
2. Gravações no SD dentro dos frames → thread de escrita em segundo plano, logs
   só em memória, depois thread de escrita no núcleo 2 com prioridade do jogo
   (P11, P16, P19).
3. Pipelines custando ~20 ms cada no console → lista de pipelines por jogo
   recriada no carregamento do mapa e uma lista semente no pacote (P12).
4. Explosões e fumaça (preenchimento de translúcidos): resolução dinâmica da
   cena 3D, corte de alfa, estabilizador que corta fumaça primeiro, orçamento
   de área de efeitos e, por fim, **fumaça e fogo numa camada de meia
   resolução** (P13–P15, P18).
5. Sprites novos decodificados duas vezes na thread do jogo → decodificação em
   workers (`vk_asynctex.cpp`) e lista de texturas de efeito pré-carregadas
   (P14).
6. Hordas (Lexicon slaughter): `malloc` da newlib caro → alocador próprio de
   objetos e de mallocs pequenos (P20); `r_ticstability` girando em espera
   ocupada → desligado (P21).
7. Resolução dinâmica: várias iterações até decidir pelo tempo da GPU
   (timestamps), por médias sustentadas e pela espera do acquire (P22).

Resultados no console: Freedoom 58–60 fps; Lexicon slaughter 59–60 fps;
Trailblazer + slaughter 55–60 fps.

Em 2026-10-13 o projeto ficou **público no GitHub**: repositório principal e
fork do motor.

## Fase 4 — 1.0.0 e 1.0.1 (2026-10-14)

- **1.0.0**: só Vulkan. O dono disse que OpenGL "nunca foi objetivo", então
  o backend GL/GLES saiu do build (`hw_noopengl.cpp` fornece os poucos símbolos
  que o motor exige). Também entrou o aviso de mods em conflito (CVARINFO
  duplicado).
- **1.0.1**: faixa morta da resolução dinâmica ajustada (desce acima de
  1,04× o orçamento e com espera acima de 4 ms incluindo o acquire; sobe só
  abaixo de 1,02×). Lexicon slaughter Insurgents (~1200 objetos) a 59–60 fps.
- CI no GitHub Actions (`.github/workflows/build.yml`).

## Fase 5 — 1.1.0 e 1.1.1: Project Brutality e WadFusion (2026-10-14)

- Modelo de perfil **Project Brutality** (`project?brutality*.pk3`).
- **Shader 2D enxuto** (`EFF_SIMPLE2D`): o visor do capacete do PB tornava o
  2D o maior custo de GPU (P23).
- **Travamento ao sair** corrigido: listas de exclusão de frames pendentes
  liberadas depois de os pools de descritores morrerem (P24).
- Imagens grandes da HUD (≥ 256×256) entram na lista de pré-carregamento
  (engasgo do traje antirradiação, P25).
- WadFusion: o arquivo do dono era para o UZDoom 4.14; regerado com o
  WadFusion v1.6.1, funciona. "Não abre" era a tela de título esperando um
  botão (P26).
- **1.1.1**: `MPG123_QUIET` (milhares de avisos de MP3 gravados no SD, P27).
- SDK do Mesa espelhado no release `deps-mesa-26.2.4-606441c9`, porque o
  artefato original expira em 2027-01-05.

O dono declarou o projeto encerrado em 2026-10-14, mas continuou mandando logs.

## Fase 6 — Manutenção: 1.1.2, 1.1.3, 1.1.4 (2026-10-15 a 10-20)

- **1.1.2**: falta de memória após ~11 min de PB + WadFusion
  (`Could not realloc 24263712 bytes`). Liberação de texturas sem uso acima de
  2300 MB de heap e orçamento de 256 MB no pré-carregamento (P28).
- **1.1.3**: o PB estava **sem efeitos sonoros desde o primeiro dia**: o
  libsndfile da imagem Docker fora compilado sem Ogg/Vorbis (P29). Também
  entrou o corte de efeitos colados na câmera (`rd_fx_nearcull`), que não
  resolveu a queda com fumaça (P31).
- **1.1.4**: travamentos de 0,8–1,2 s a cada 30 s. Eram a própria liberação de
  texturas, destruindo milhares de imagens num só frame. Agora sai aos poucos,
  4 por quadro. O visor do traje antirradiação fica sempre pronto (P30). A
  queda com fumaça e explosões foi diagnosticada como CPU (lógica do PB), sem
  solução no port (P31).

Depois da 1.1.4 o dono descartou dois commits feitos com o aider (um adicionava
e o outro removia um caminho de logs, sem efeito líquido): o histórico do `main`
foi reescrito sem eles e a tag `v1.1.4` aponta para o commit `adc1634`. O
projeto foi então dado como **concluído**.

## Releases

| Versão | Data | Motor | Destaque |
|---|---|---|---|
| 1.0.0 | 2026-10-14 | 5.0.3-56-g499d2922b | só Vulkan, aviso de mods em conflito |
| 1.0.1 | 2026-10-14 | 5.0.3-59-g2af10015c | resolução dinâmica segura 60 fps |
| 1.1.0 | 2026-10-14 | 5.0.3-62-g911f28b46 | PB, WadFusion, shader 2D, sem crash ao sair |
| 1.1.1 | 2026-10-14 | 5.0.3-63-g1c1599ca7 | mpg123 silencioso |
| 1.1.2 | 2026-10-15 | 5.0.3-64-g9c4078c49 | liberação de texturas, sem falta de memória |
| 1.1.3 | 2026-10-16 | 5.0.3-65-g2e72b192e | sons Ogg/FLAC/Opus, corte de fumaça na câmera |
| 1.1.4 | 2026-10-20 | 5.0.3-66-g1eb49010f | liberação de texturas sem travar |
