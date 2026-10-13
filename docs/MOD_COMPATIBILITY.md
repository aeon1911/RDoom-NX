# Compatibilidade de mods — RDoom NX (UZDoom 5.0.3)

Testes feitos no Ryujinx 1.3.3 com o `RDoom.nro` empacotado, pelo launcher,
com o controle virtual (`scripts/vpad.py`). Os arquivos são os do usuário,
ligados por symlink no SD de teste (`test-env/local-content.txt`); nada disso
é distribuído. **Switch físico: pendente.**

| Conteúdo | Versão / arquivo | Vulkan | OpenGL | Evidência |
|---|---|---|---|---|
| Freedoom: Phase 2 | 0.13.0, `freedoom2.wad` | ✅ título, menu, MAP01, andar, mirar, atirar | ✅ | `evidence/ryujinx/32-vk-freedoom`, `33-vk-play`, `36-vk-overlap`; `18-opengl-play` |
| Brutal Doom | v22 test 6, `brutal22test6.pk3`, sobre DOOM II | ✅ MAP01, tiros, chute, movimento, 7200 frames sem erro | ✅ | `34-vk-brutal`, `bench-brutal-map01-*` |
| Lexicon | 1.0, `lexicon-core-v1.0.pk3` + `lexicon-base-v1.0.pk3`, DOOM II | ✅ título, menu, hub "VR - Inside the Lexicon", movimento | não repetido | `35-vk-lexicon` |
| The Compendium | v1a, `Compendium-v1a.pk3` + `compen-play.pk3`, DOOM II | ✅ HUBMAP (seletor), movimento | não repetido | `35-vk-compendium` |
| WadFusion | `doom_fusion.ipk3` **gerado para UZDoom/GZDoom 4.14** | ❌ (motor recusa o arquivo) | ❌ | `31-vk-blitupload/wadfusion-uzdoom-5.0.3-linux-pc.log` |

Os testes não mostraram erros de script (`engine.log` sem "error") em
Brutal Doom, Lexicon e Compendium.

## WadFusion: por que falha e como resolver

O `doom_fusion.ipk3` do usuário foi gerado por uma versão do WadFusion feita
para o UZDoom/GZDoom 4.14: o ZScript dele declara atores ID24 que o UZDoom
5.0.3 já traz (`ID24LargeCorpsePile` etc.) e usa `compat_nombf21`, removido no
5.0. O motor para com **108 erros** ao compilar `zscript.zs`.

Não é um problema do port: o **UZDoom 5.0.3 oficial para Linux (PC)** dá
exatamente os mesmos 108 erros com o mesmo arquivo (log salvo em
`evidence/ryujinx/31-vk-blitupload/wadfusion-uzdoom-5.0.3-linux-pc.log`).

Solução: gerar de novo o `doom_fusion.ipk3` com o **WadFusion v1.6.1**
(`wadfusion_py.zip`, versão compatível com UZDoom 5) a partir dos seus próprios
WADs comerciais, e copiar para `sdmc:/switch/rdoom/iwads/`. Os WADs
originais não estão neste ambiente, por isso a regeneração não foi testada.

O launcher reconhece esse caso pelo conteúdo do arquivo (ZScript com
`id24gore.zs` + `id24nature.zs` próprios) e mostra o perfil como **Erro**,
com essa explicação, em vez de deixar o motor falhar.

## Observações

- "Contaminação de perfil" vista num teste antigo (WadFusion carregado no
  lugar do Brutal Doom, `evidence/ryujinx/21-brutal`) era do roteiro de teste:
  o launcher abre no último perfil usado e o roteiro não escolhia o perfil. O
  `scripts/ryujinx-test.sh` agora define o perfil (`PROFILE=`) antes de cada
  execução; o launcher em si sempre carregou exatamente os arquivos do perfil
  escolhido (ver `last-launch.txt` de cada teste).
- Modo applet (álbum) tem pouca memória: mods grandes devem ser abertos em
  modo título (segurar R ao abrir um jogo). O launcher avisa.
- Mods feitos para GZDoom ≤ 4.14 podem depender de recursos removidos no
  UZDoom 5; verifique a versão recomendada pelo autor do mod.
