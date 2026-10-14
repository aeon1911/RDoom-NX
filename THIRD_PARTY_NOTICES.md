# Third-party notices — RDoom NX

RDoom is distributed under the **GNU General Public License v3** (the licence
of UZDoom, on which it is based). The full text is in `engine/LICENSE`
(packaged as `switch/rdoom/licenses/GPL-3.0-UZDoom.txt`). The complete source
code, including every RDoom change, is in this repository: `engine/` (UZDoom
5.0.3 plus the patch series in `patches/`), `launcher/`, `vktest/`, `scripts/`.

RDoom is an unofficial port. It is not a product of id Software, Bethesda,
ZeniMax, Nintendo, or the UZDoom/GZDoom maintainers. "Doom" is a trademark of
its owners; no Doom logo, character art or commercial game data is included.

## Components linked into RDoom.nro

| Component | Version / revision | Licence | Where |
|---|---|---|---|
| UZDoom | tag 5.0.3, `7910df7102033fd7754a317f3254f24101135926` | GPL-3.0-or-later (engine), with code under BSD/MIT/zlib and others listed in `engine/docs/licenses/` | `engine/` |
| GZDoom / ZDoom lineage | via UZDoom | GPL-3.0-or-later and the historic Doom source licence terms in UZDoom | `engine/` |
| Earlier Switch port of GZDoom (fgsfds) — platform code, crash handler, loading screen base | via the user's UZDoom 5.0.3 Switch port | BSD-3-Clause style (file headers) / GPL-3.0 | `engine/src/common/platform/posix/nx/` |
| Mesa (NVK Vulkan, NAK; EGL linked by SDL2, not used for rendering) — Horizon port | Mesa 26.2.4, danfromtico/mesa-switch `606441c90e459028a50fca352daebfc20211f890` (CI artifact 11484346771, sha256 `937cbda8…ca392`) | MIT (most of Mesa; see `MESA-MIT.txt`), some components Apache-2.0 / BSL-1.0 / SGI-B-2.0 (see the mesa-switch `licenses/` folder) | linked static |
| Rust standard library (inside the Mesa SDK, used by NAK) | rustc nightly used by the SDK build | MIT OR Apache-2.0 | linked static |
| libnx | 4.12.0 (devkitPro image `devkita64:20260219`) | ISC (`LIBNX-ISC.txt`) | linked static |
| devkitA64 runtime (newlib, libgcc, libstdc++) | GCC 15.2.0 toolchain | newlib: BSD-style licences; libgcc/libstdc++: GPL-3.0 with the GCC Runtime Library Exception | linked static |
| SDL2 (switch-sdl2) | 2.28.5 | zlib | linked static |
| OpenAL Soft (switch-openal-soft) | 1.21.1 | LGPL-2.1-or-later | linked static (satisfied by full source availability) |
| libsndfile | 1.2.2 (`72f6af15e8f85157bd622ed45b979025828b7001`) | LGPL-2.1-or-later | linked static |
| mpg123 | 1.31.3 | LGPL-2.1 | linked static |
| libogg / libvorbis / FLAC / Opus / opusfile | devkitPro portlibs | BSD-3-Clause | linked static |
| zlib, zstd, expat | devkitPro portlibs | zlib / BSD-3-Clause / MIT | linked static |
| ZMusic, FluidSynth, TiMidity++, libADLMIDI, libOPNMIDI, WildMidi | bundled in UZDoom 5.0.3 | LGPL/GPL/zlib/BSD as listed in `engine/libraries/ZMusic/licenses/` | linked static |
| ZVulkan, VMA, glslang, volk (bundled with UZDoom) | UZDoom 5.0.3 | MIT / BSD-3-Clause / Apache-2.0 (glslang parts) | linked static |
| nlohmann/json | 3.11.3 (sha256 `9bea4c80…103ea6`) | MIT (`NLOHMANN-JSON-MIT.txt`) | `launcher/third_party/json.hpp` |
| stb_image 2.30, stb_truetype 1.26, stb_image_write 1.16 | nothings/stb | MIT or Public Domain (`STB-LICENSE.txt`) | `launcher/third_party/` |
| Bebas Neue, Poppins (fonts) | from the earlier Switch port | SIL Open Font License 1.1 (`OFL-BebasNeue.txt`, `OFL-Poppins.txt`) | embedded in the launcher |

## Shipped data

| Data | Licence | Notes |
|---|---|---|
| `uzdoom.pk3`, `game_support.pk3`, `game_widescreen_gfx.pk3`, `brightmaps.pk3`, `lights.pk3`, `soundfonts/uzdoom.sf2`, `fm_banks/*` | as in UZDoom 5.0.3 | built unchanged from `engine/wadsrc*` |
| RDoom artwork (`launcher/*.jpg`, `*.png`, icon) | GPL-3.0 (part of RDoom) | original vector/procedural art from `assets/src/generate_art.py`; not AI-generated (see `assets/prompts/README.md`) |
| Freedoom 0.13.0 `freedoom1.wad`, `freedoom2.wad` — only in `RDoom-NX-<ver>-Freedoom.zip` | BSD-3-Clause style (`licenses/freedoom/COPYING.txt`), credits in `CREDITS.txt` | official release, sha256 of the zip `3f9b264f…59` verified against the signed CHECKSUM file |

No commercial IWAD, mod or extracted game asset is distributed. Users supply
their own IWADs and mods; `tests/check_package.py` rejects any IWAD in the
package other than the two Freedoom files above.

## Tools used to build (not distributed)

devkitPro `devkita64:20260219` Docker image (digest
`sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282`),
CMake, Ninja, rsvg-convert, ImageMagick, glslc (vktest shaders).
