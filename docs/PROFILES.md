# Perfis do RDoom NX

Um perfil é tudo o que define uma sessão de jogo: IWAD, mods e sua ordem de
carregamento, renderizador, vídeo, controles, opções de jogo e CVars extras.
Cada perfil é um arquivo JSON em `sdmc:/switch/rdoom/profiles/<id>.json`, tem
a sua configuração do motor (`config/engine/<id>.ini`) e a sua pasta de saves
(`saves/<id>/`). Assim, mods diferentes nunca misturam configurações ou saves.

## Modelos (criados na primeira execução)

| id | Nome | IWAD aceita (ordem de preferência) | Mods procurados em `mods/` |
|---|---|---|---|
| `freedoom2-vulkan-stock` | FreeDoom 2 — Vulkan Stock | `freedoom2.wad` | — |
| `brutal-doom` | Brutal Doom | `doom2.wad`, `doom.wad`, `freedoom2.wad` | `brutal*.pk3` |
| `wadfusion` | WadFusion | `doom_fusion.ipk3` (como IWAD) | `doom_fusion_widescreen_gfx.pk3` (opcional) |
| `lexicon` | Lexicon | `doom2.wad` | `lexicon-core*.pk3`, `lexicon-base*.pk3` |
| `compendium` | Compendium | `doom2.wad` | `compendium*.pk3`, `compen-play*.pk3` (opcional) |
| `personalizado` | Configuração personalizada | escolha livre | escolha livre |

Os modelos só apontam para arquivos; nenhum conteúdo comercial é distribuído.
Quando um arquivo exigido não está no SD, o perfil aparece como **"Pendente
de instalação"** e o launcher diz qual arquivo falta. Um conteúdo reconhecido
como incompatível com o UZDoom 5.0.3 aparece como **Erro**, com a explicação
(caso do WadFusion gerado para o UZDoom 4.14 — ver `MOD_COMPATIBILITY.md`).

Todos os modelos usam a configuração padrão **"RDoom — Stock Switch"**:
Vulkan, escala interna 1,0, vsync ligado, limite de 60 FPS, qualidade
`stock` (sem FXAA/SSAO/bloom/lens/tonemap, 1 amostra), luzes dinâmicas
ligadas, filtro de textura 0, sem nenhuma alteração de clock.

## Operações no launcher

Criar (a partir de um modelo), duplicar, renomear, editar, apagar, exportar,
importar e restaurar padrões. Toda escrita é atômica (arquivo temporário +
cópia `.bak` da versão anterior + rename). Apagar remove o JSON e o `.ini` do
motor; **saves, IWADs e mods nunca são apagados**. Exportar grava em
`export/`; importar valida o arquivo inteiro antes de aceitar. Um JSON
corrompido é copiado para `backup/` e relatado, nunca apagado.

Ordem de carregamento: na aba Mods, **A** liga/desliga um mod, **Y** entra no
modo mover (direcional muda a posição), **X** remove da lista, **+** adiciona.
O motor recebe os mods exatamente nessa ordem (`-file`).

## Formato (versão 1)

```json
{
  "format": "rdoom-profile",
  "version": 1,
  "id": "brutal-doom",
  "name": "Brutal Doom",
  "description": "…",
  "template": "brutaldoom",
  "iwad": "DOOM2.WAD",
  "iwad_candidates": ["doom2.wad", "doom.wad", "freedoom2.wad"],
  "mods": [ { "file": "brutal22test6.pk3", "enabled": true, "optional": false } ],
  "mod_patterns": ["brutal*.pk3"],
  "video": {
    "renderer": "vulkan",          // "vulkan" | "opengl" | "gles"
    "internal_scale": 1.0,         // vid_scalefactor 0.5–1.0
    "fps_limit": 60,               // vid_maxfps, 0 = sem limite, senão 20–60
    "vsync": true,                 // vid_vsync
    "quality": "stock",            // "low" | "stock" | "high"
    "dynamic_lights": true,        // r_dynlights
    "texture_filter": 0            // gl_texture_filter
  },
  "input": { "look_sensitivity": 1.0, "deadzone": 0.15, "invert_look": false },
  "gameplay": { "skill": 0, "warp": "", "autoaim": 35.0, "always_run": true,
                "crosshair": 1, "fast": false, "no_monsters": false, "respawn": false },
  "cvars": { },                    // "+set nome valor" extras, nomes/valores validados
  "created": "2026-10-11T11:28:44Z",
  "modified": "2026-10-11T12:52:04Z",
  "app_version": "RDoom 1.0.0"
}
```

Leitura tolerante: campos ausentes ou com tipo errado ficam no padrão e geram
aviso; arquivos com `version` antiga passam por migração (com backup). Os
nomes e valores de `cvars` só aceitam caracteres seguros, então um perfil
importado não consegue injetar comandos.

## Do perfil ao motor

`launcher/src/core/launch.cpp`:

- `vid_preferbackend` é gravado em `[GlobalSettings]` de
  `config/engine/<id>.ini` (0 = OpenGL, 1 = Vulkan, 2 = OpenGL ES). O motor lê
  esse valor antes de criar a janela; o relatório `logs/engine-session.txt`
  mostra `renderer_requested` e `renderer_active`.
- O resto vira `+set` na linha de comando: `vid_scalefactor`, `vid_maxfps`,
  `vid_vsync`, `r_dynlights`, `gl_texture_filter`, efeitos de pós-processamento
  conforme a qualidade, `rd_joy_sensitivity`/`rd_joy_deadzone`/
  `rd_joy_invert_look`, `autoaim`, `cl_run`, `crosshair` e os `cvars` extras.
- `-iwad`, `-file` (na ordem), `-config`, `-savedir`, `-skill`, `-fast`,
  `-nomonsters`, `-respawn` e `+map <warp>` quando o perfil define um mapa.

O último comando de início fica em `logs/last-launch.txt` (também visível na
tela Diagnóstico) e o plano detalhado em `logs/rdoom.log`.
