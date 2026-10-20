# docs/ai — conhecimento do projeto para IAs

Conjunto escrito ao encerrar o projeto (2026-10-20, versão 1.1.4) para que
qualquer IA que assuma o RDoom NX entenda tudo o que foi feito, os erros
enfrentados e como foram resolvidos. A porta de entrada é o `AGENTS.md` na
raiz, com as regras do dono.

| Arquivo | Conteúdo |
|---|---|
| `01-HISTORIA.md` | linha do tempo do início ao fim, fases e releases |
| `02-PROBLEMAS-E-SOLUCOES.md` | catálogo P01–P44: sintoma, causa, investigação, correção, lição |
| `03-MAPA-DO-CODIGO.md` | estrutura do repositório, arquivos do port, sistemas de desempenho, CVARs, layout do SD |
| `04-FLUXOS-DE-TRABALHO.md` | build, testes de host, Ryujinx, análise de logs do console, release |
| `05-LIMITES-E-ARMADILHAS.md` | limites sem solução, becos sem saída, armadilhas do emulador e do console |

Fontes complementares (pt-BR): `docs/STATUS.md` (cada teste no console, em
ordem), `docs/VULKAN.md`, `docs/ARCHITECTURE.md`, `docs/PERFORMANCE.md`,
`docs/PORTING_AUDIT.md`, `docs/PROFILES.md`, `docs/MOD_COMPATIBILITY.md`,
`docs/RYUJINX_TEST_REPORT.md`. As mensagens de commit do motor
(`git -C engine log 7910df71..HEAD`) explicam cada mudança com medições.

Os logs de evidência (`evidence/`) e o ambiente do emulador (`test-env/`)
existem só na máquina do dono e não estão no GitHub.

Ao continuar o projeto, mantenha este conjunto atualizado: nova fase em
`01-HISTORIA.md`, novo problema em `02-PROBLEMAS-E-SOLUCOES.md` (próximo número
livre) e limites revistos em `05-LIMITES-E-ARMADILHAS.md`.
