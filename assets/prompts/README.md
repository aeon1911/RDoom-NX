# RDoom artwork: origin and AI prompts

## Status of AI generation: **pending**

On 2026-10-11 the build machine was checked for an image generation tool the
agent could actually use:

| Checked | Result |
|---|---|
| Image-generation tools exposed to the agent | none |
| Ollama 0.40.1 (`/usr/bin/ollama`) | installed, no models; the binary reports "image generation models are not currently supported" |
| Python diffusers / torch / ComfyUI / Stable Diffusion installs | none found |
| `photocraft-0.6.0` AppImage on the build machine | GUI photo editor, no scriptable generation |

No API key was requested. Following the project brief, the artwork shipped
with RDoom is **original vector/procedural art**, drawn by
`assets/src/generate_art.py` (SVG + `feTurbulence` noise, rendered with
`rsvg-convert`, sized with ImageMagick). It is **not** AI-generated and must not
be described as such.

The prompts below are ready for when a generator is available. Replacing a
file keeps its name and size (see the table), so the launcher needs no change.

## Files, sizes and where they are used

| File (assets/build) | Size | Used by | Source |
|---|---|---|---|
| `bg_main.jpg` | 1280x720 | launcher home screen | `generate_art.py: background_main` |
| `bg_inner.jpg` | 1280x720 | library / settings screens | `background_inner` |
| `card_<template>.jpg` | 480x270 | profile cards | `card_svg` |
| `logo_620.png`, `logo_300.png` | transparent | launcher header, loading screen | `logo_svg` (hand-drawn letter polygons) |
| `rdoom_logo.png` / `_flat.png` / `_dark.jpg` | 1400x420 | documentation, promo | `logo_svg` |
| `icon_256.jpg` | 256x256 | NRO icon (homebrew menu) | `icon_svg` |
| `splash.jpg`, `promo_1280x720.jpg` | 1280x720 | loading screen, docs | composites |

The lettering "RDoom" is drawn as polygons in `GLYPHS` (no font involved), so
the text is exact at every size. No id Software logo, character or cover art
is reproduced.

## Prompts (for a future AI pass)

See `prompts.txt` in this folder.
