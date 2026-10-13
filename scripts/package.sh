#!/usr/bin/env bash
# Assembles the SD card package from build/nx and assets/build.
#
#   dist/RDoom-NX/                      tree to copy to the SD card root
#   dist/RDoom.nro                      the homebrew alone
#   dist/RDoom-NX-<ver>.zip             extract to the SD card root
#   dist/RDoom-NX-<ver>-Freedoom.zip    same + Freedoom 0.13.0 IWADs (BSD licence)
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
B="$ROOT/build/nx"
VER=$(sed -n 's/.*set( RDOOM_VERSION "\(.*\)" ).*/\1/p' "$ROOT/engine/CMakeLists.txt")
OUT="$ROOT/dist/RDoom-NX"
SD="$OUT/switch/rdoom"

[ -f "$B/RDoom.nro" ] || { echo "build/nx/RDoom.nro missing: run scripts/build.sh" >&2; exit 1; }
rm -rf "$OUT" "$ROOT/dist/RDoom.nro" "$ROOT"/dist/RDoom-NX-*.zip
mkdir -p "$SD"/{iwads,mods,launcher,licenses}

# engine + launcher
cp "$B/RDoom.nro" "$SD/"
for pk3 in uzdoom.pk3 game_support.pk3 game_widescreen_gfx.pk3 brightmaps.pk3 lights.pk3; do
	cp "$B/$pk3" "$SD/"
done
cp -r "$B/soundfonts" "$B/fm_banks" "$SD/"

# launcher artwork (loaded from the SD card, not embedded in the NRO)
for f in bg_main.jpg bg_inner.jpg splash.jpg logo_300.png logo_400.png logo_620.png \
         card_freedoom2.jpg card_brutaldoom.jpg card_wadfusion.jpg card_lexicon.jpg card_compendium.jpg card_custom.jpg; do
	cp "$ROOT/assets/build/$f" "$SD/launcher/"
done

# user folders with instructions (no game data is shipped here)
cat > "$SD/iwads/LEIA-ME.txt" <<'EOF'
Coloque aqui as IWADs (jogos base) que voce possui:
  freedoom1.wad, freedoom2.wad (livres: https://freedoom.github.io)
  doom.wad, doom2.wad, plutonia.wad, tnt.wad, heretic.wad, hexen.wad ... (comerciais, copie as suas)
  doom_fusion.ipk3 (WadFusion, gerado a partir dos seus arquivos)
O RDoom nao inclui IWADs comerciais.
EOF
cat > "$SD/mods/LEIA-ME.txt" <<'EOF'
Coloque aqui mods e pacotes de mapas (.pk3 .wad .zip .pk7 .deh .bex).
Subpastas sao permitidas (ex.: mods/lexicon/lexicon-core-v1.0.pk3).
Nada aqui e apagado pelo RDoom: remover um mod de um perfil so tira a referencia.
EOF

# diagnostics template (inactive under this name; renamed to rdoom_debug.txt it is applied at start-up)
cat > "$SD/rdoom_debug.exemplo.txt" <<'EOF'
# Diagnostico do RDoom. Para usar, renomeie este arquivo para rdoom_debug.txt
# (na mesma pasta) e apague o "#" da linha que quiser ativar.
# Linhas em MAIUSCULAS = variaveis do driver Mesa; em minusculas = CVARs do motor.
# O que foi aplicado aparece em logs/engine-session.txt (linha debug_file).
#
# Benchmark de GPU: entre num mapa e nao mexa no controle por ~2 minutos; o jogo
# congela, testa cada opcao em 4 direcoes e grava logs/gpu-bench.txt:
#rd_gpubench=1
#
# Formato da imagem da cena: 0 = RGBA16F (padrao), 1 = RGB10A2, 2 = RGBA8
# (menos trafego de memoria na GPU; bloom/tonemap ficam sem HDR):
#rd_vk_scenefmt=2
#
# Desliga a sobreposicao CPU/GPU entre frames (teste de estabilidade):
#rd_vk_frameoverlap=0
#
# Cada envio a GPU espera terminar; numa falha, o driver mostra o comando que a causou
# (mais lento, so para diagnostico; o resultado vai para logs/engine-stderr.log):
#NVK_DEBUG=push_sync
#
# Desliga as luzes dinamicas:
#r_dynlights=0
#
# Profundidade em D32S8 em vez de D24S8:
#rd_vk_depth32=1
#
# Modo rastreamento (descobre qual desenho trava a GPU; muito lento): ative as 3 linhas abaixo
# junto com NVK_DEBUG=push_sync acima. Depois de travar, mande logs/boot.log.
#vk_submit_size=1
#rd_vk_drawlog=2
#rd_vk_frameoverlap=0
EOF

# licences
cp "$ROOT/engine/LICENSE" "$SD/licenses/GPL-3.0-UZDoom.txt"
# the NRO icon and the launcher background are built from Freedoom 2 graphics (BSD)
mkdir -p "$SD/licenses/freedoom-art"
cp "$ROOT/third_party/freedoom/freedoom-0.13.0/COPYING.txt" "$ROOT/third_party/freedoom/freedoom-0.13.0/CREDITS.txt" "$SD/licenses/freedoom-art/"
printf 'O icone do RDoom.nro e o fundo do launcher usam graficos do Freedoom 2 (TITLEPIC, BOSSBACK, HEADA1, PLAYF1),\ndistribuidos sob a licenca BSD em COPYING.txt. Creditos dos artistas em CREDITS.txt.\nhttps://freedoom.github.io\n' > "$SD/licenses/freedoom-art/LEIA-ME.txt"
cp -r "$ROOT/engine/docs/licenses" "$SD/licenses/uzdoom-third-party"
cp "$ROOT/THIRD_PARTY_NOTICES.md" "$SD/licenses/"
cp "$ROOT/launcher/res/fonts/OFL-BebasNeue.txt" "$ROOT/launcher/res/fonts/OFL-Poppins.txt" "$SD/licenses/"
cp "$ROOT/third_party/licenses/"* "$SD/licenses/" 2>/dev/null || true

# version + readme
{
	echo "RDoom $VER"
	echo "build: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
	echo "engine: UZDoom 5.0.3 (7910df7102033fd7754a317f3254f24101135926) + RDoom patches $(git -C "$ROOT/engine" describe --always --dirty --tags)"
	echo "graphics: Mesa 26.2.4 Horizon SDK (danfromtico/mesa-switch 606441c90e459028a50fca352daebfc20211f890), Vulkan via NVK"
	echo "RDoom.nro sha256: $(sha256sum "$SD/RDoom.nro" | cut -d' ' -f1)"
	echo "clocks: stock only (no overclock code, no sys-clk dependency)"
} > "$SD/VERSION.txt"
cp "$ROOT/docs/INSTALL-pt-BR.txt" "$OUT/LEIA-ME.txt"

cp "$SD/RDoom.nro" "$ROOT/dist/RDoom.nro"
python3 -I "$ROOT/scripts/mkzip.py" "$ROOT/dist/RDoom-NX-$VER.zip" "$OUT" switch LEIA-ME.txt

# variant with the free Freedoom IWADs, ready to play
FD="$ROOT/third_party/freedoom/freedoom-0.13.0"
if [ -f "$FD/freedoom2.wad" ]; then
	TMP=$(mktemp -d "$ROOT/build/pkg-XXXX")
	cp -r "$OUT/switch" "$OUT/LEIA-ME.txt" "$TMP/"
	cp "$FD/freedoom1.wad" "$FD/freedoom2.wad" "$TMP/switch/rdoom/iwads/"
	mkdir -p "$TMP/switch/rdoom/licenses/freedoom"
	cp "$FD/COPYING.txt" "$FD/CREDITS.txt" "$FD/CREDITS-MUSIC.txt" "$TMP/switch/rdoom/licenses/freedoom/"
	python3 -I "$ROOT/scripts/mkzip.py" "$ROOT/dist/RDoom-NX-$VER-Freedoom.zip" "$TMP" switch LEIA-ME.txt
	rm -rf "$TMP"
fi
(cd "$ROOT/dist" && sha256sum RDoom.nro RDoom-NX-*.zip > SHA256SUMS.txt)
echo "package: $OUT"
ls -l "$ROOT/dist"
