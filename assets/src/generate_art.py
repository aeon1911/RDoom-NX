#!/usr/bin/env python3
"""RDoom NX artwork generator.

Everything here is original, procedurally/vector drawn artwork (no AI model was
available on the build machine; see assets/prompts/README.md). The script
writes SVG sources to assets/src/svg/ and renders them with rsvg-convert, then
ImageMagick produces the sizes the launcher and the NRO icon need.

    python3 assets/src/generate_art.py          # writes assets/build/*

Deterministic: the same script always produces the same images.
"""
import math
import re
import os
import random
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SVG_DIR = os.path.join(ROOT, "src", "svg")
OUT = os.path.join(ROOT, "build")

# ---------------------------------------------------------------------------
# lettering: "RDoom", drawn as polygons on a 200-unit cap height grid
# ---------------------------------------------------------------------------

GLYPHS = {
    # (advance width, outer contour, counters)
    "R": (150,
          "M0,0 H128 L150,22 V86 L134,104 L152,200 H110 L94,118 H38 V200 H0 Z",
          ["M38,32 H106 L114,40 V70 L106,80 H38 Z"]),
    "D": (152,
          "M0,0 H112 L152,40 V160 L112,200 H0 Z",
          ["M38,34 H96 L114,52 V148 L96,166 H38 Z"]),
    "o": (132,
          "M22,60 H110 L132,82 V178 L110,200 H22 L0,178 V82 Z",
          ["M38,94 H94 V166 H38 Z"]),
    "m": (204,
          "M0,60 H182 L204,82 V200 H165 V96 H121 V200 H83 V96 H38 V200 H0 Z",
          []),
}
SPACING = 16


def word_path(text):
    """Returns (svg path data, width) for text laid out on the glyph grid."""
    parts = []
    x = 0
    for ch in text:
        adv, outer, counters = GLYPHS[ch]
        for contour in [outer] + counters:
            parts.append(translate_path(contour, x, 0))
        x += adv + SPACING
    return " ".join(parts), x - SPACING


def translate_path(d, dx, dy):
    out = []
    tokens = re.findall(r"[A-Za-z]|-?[0-9.]+", d)
    i = 0
    cmd = None
    while i < len(tokens):
        t = tokens[i]
        if t[0].isalpha():
            cmd = t
            out.append(t)
            i += 1
            continue
        if cmd in ("M", "L"):
            out.append("%g,%g" % (float(tokens[i]) + dx, float(tokens[i + 1]) + dy))
            i += 2
        elif cmd == "H":
            out.append("%g" % (float(t) + dx))
            i += 1
        elif cmd == "V":
            out.append("%g" % (float(t) + dy))
            i += 1
        else:
            raise ValueError(cmd)
    return " ".join(out)


LOGO_DEFS = """
  <linearGradient id="steel" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#fff4e0"/>
    <stop offset="0.18" stop-color="#d9c7b0"/>
    <stop offset="0.46" stop-color="#7c6f66"/>
    <stop offset="0.50" stop-color="#3a3130"/>
    <stop offset="0.74" stop-color="#8a2a10"/>
    <stop offset="1" stop-color="#ff7a1e"/>
  </linearGradient>
  <linearGradient id="rim" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#ffd27a"/>
    <stop offset="1" stop-color="#ff3b0a"/>
  </linearGradient>
  <filter id="glow" x="-20%" y="-40%" width="140%" height="180%">
    <feGaussianBlur stdDeviation="14" result="b"/>
    <feColorMatrix in="b" type="matrix" values="0 0 0 0 1  0 0 0 0 0.22  0 0 0 0 0.02  0 0 0 1.4 0"/>
  </filter>
  <filter id="grime" x="0" y="0" width="100%" height="100%">
    <feTurbulence type="fractalNoise" baseFrequency="0.9 0.35" numOctaves="3" seed="7" result="n"/>
    <feColorMatrix in="n" type="matrix" values="0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  0 0 0 -2.2 1.25" result="m"/>
    <feComposite in="SourceGraphic" in2="m" operator="in"/>
  </filter>
"""


def logo_svg(width_px, height_px, with_glow=True, plate=False, extra_scale=1.0):
    path, w = word_path("RDoom")
    h = 200
    # italic shear and the 3D extrusion need room around the letters
    vb_w, vb_h = w + 140, h + 120
    s = []
    s.append('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="%g %g %g %g">'
             % (width_px, height_px, -70, -60, vb_w, vb_h))
    s.append("<defs>%s</defs>" % LOGO_DEFS)
    if plate:
        s.append('<rect x="-70" y="-60" width="%g" height="%g" fill="#0b0506"/>' % (vb_w, vb_h))
    tr = 'transform="translate(%g,0) skewX(-10) scale(%g)"' % (h * 0.1, extra_scale)
    s.append("<g %s>" % tr)
    if with_glow:
        s.append('<path d="%s" fill="#ff4a10" fill-rule="evenodd" filter="url(#glow)"/>' % path)
    # extrusion: stacked dark copies down-right
    for i in range(10, 0, -1):
        shade = 20 + i * 3
        s.append('<path d="%s" transform="translate(%g,%g)" fill="rgb(%d,%d,%d)" fill-rule="evenodd"/>'
                 % (path, i * 0.9, i * 1.1, shade + 30, shade // 3, shade // 4))
    s.append('<path d="%s" fill="none" stroke="#050202" stroke-width="10" stroke-linejoin="miter" fill-rule="evenodd"/>' % path)
    s.append('<path d="%s" fill="url(#steel)" fill-rule="evenodd"/>' % path)
    s.append('<path d="%s" fill="#1a0a08" fill-rule="evenodd" opacity="0.55" filter="url(#grime)"/>' % path)
    s.append('<path d="%s" fill="none" stroke="url(#rim)" stroke-width="2.5" fill-rule="evenodd" opacity="0.9"/>' % path)
    # a cut line across the letters, as if machined
    s.append('<rect x="-10" y="96" width="%g" height="5" fill="#0a0303" opacity="0.65"/>' % (w + 20))
    s.append("</g></svg>")
    return "\n".join(s)


# ---------------------------------------------------------------------------
# scenes
# ---------------------------------------------------------------------------

def embers(rng, n, w, h, ymin, ymax, rmax=2.6):
    out = []
    for _ in range(n):
        x = rng.uniform(0, w)
        y = rng.uniform(ymin, ymax)
        r = rng.uniform(0.6, rmax)
        a = rng.uniform(0.35, 1.0)
        col = rng.choice(["#ffb347", "#ff7a1e", "#ff4a10", "#ffd27a"])
        out.append('<circle cx="%.1f" cy="%.1f" r="%.2f" fill="%s" opacity="%.2f"/>' % (x, y, r, col, a))
    return "\n".join(out)


def industrial_skyline(rng, w, base_y, color, rim, scale=1.0, density=1.0):
    """Towers, tanks, pipes and gantries as silhouettes with a hot rim light."""
    s = []
    x = -40
    while x < w + 40:
        kind = rng.random()
        if kind < 0.35:  # tower with chimney
            tw = rng.uniform(40, 90) * scale
            th = rng.uniform(120, 260) * scale
            s.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s"/>' % (x, base_y - th, tw, th, color))
            cw = tw * 0.28
            ch = rng.uniform(40, 110) * scale
            cx = x + tw * rng.uniform(0.15, 0.6)
            s.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s"/>' % (cx, base_y - th - ch, cw, ch, color))
            s.append('<rect x="%.1f" y="%.1f" width="%.1f" height="2" fill="%s" opacity="0.8"/>' % (cx, base_y - th - ch, cw, rim))
            s.append('<rect x="%.1f" y="%.1f" width="2" height="%.1f" fill="%s" opacity="0.55"/>' % (x + tw - 2, base_y - th, th, rim))
            # lit windows
            for wy in range(int(base_y - th + 14), int(base_y - 10), int(22 * scale) or 1):
                if rng.random() < 0.35:
                    s.append('<rect x="%.1f" y="%d" width="%.1f" height="3" fill="#ff8a2a" opacity="%.2f"/>'
                             % (x + tw * 0.2, wy, tw * 0.6, rng.uniform(0.25, 0.8)))
            x += tw + rng.uniform(6, 30) * scale / density
        elif kind < 0.6:  # tank
            r = rng.uniform(30, 60) * scale
            s.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" rx="%.1f" fill="%s"/>'
                     % (x, base_y - r * 2.2, r * 2, r * 2.2, r * 0.9, color))
            s.append('<path d="M%.1f,%.1f a%.1f,%.1f 0 0 1 %.1f,0" fill="none" stroke="%s" stroke-width="2" opacity="0.6"/>'
                     % (x + 3, base_y - r * 1.4, r - 3, r * 0.8, 2 * r - 6, rim))
            x += r * 2 + rng.uniform(4, 24) * scale / density
        elif kind < 0.8:  # gantry / catwalk
            gw = rng.uniform(90, 180) * scale
            gh = rng.uniform(60, 140) * scale
            for k in range(0, int(gw), max(8, int(18 * scale))):
                s.append('<rect x="%.1f" y="%.1f" width="3" height="%.1f" fill="%s"/>' % (x + k, base_y - gh, gh, color))
            s.append('<rect x="%.1f" y="%.1f" width="%.1f" height="6" fill="%s"/>' % (x, base_y - gh, gw, color))
            s.append('<rect x="%.1f" y="%.1f" width="%.1f" height="1.5" fill="%s" opacity="0.7"/>' % (x, base_y - gh, gw, rim))
            x += gw + rng.uniform(0, 20) * scale / density
        else:  # pipe run
            pw = rng.uniform(80, 200) * scale
            py = base_y - rng.uniform(20, 90) * scale
            s.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s"/>' % (x, py, pw, 10 * scale, color))
            s.append('<rect x="%.1f" y="%.1f" width="%.1f" height="1.5" fill="%s" opacity="0.6"/>' % (x, py, pw, rim))
            x += pw * 0.6
    s.append('<rect x="0" y="%.1f" width="%d" height="400" fill="%s"/>' % (base_y - 1, w, color))
    return "\n".join(s)


BG_DEFS = """
  <radialGradient id="sky" cx="0.62" cy="0.92" r="0.95">
    <stop offset="0" stop-color="#ff6a1c"/>
    <stop offset="0.16" stop-color="#c2280c"/>
    <stop offset="0.42" stop-color="#4a0b07"/>
    <stop offset="0.75" stop-color="#170405"/>
    <stop offset="1" stop-color="#070203"/>
  </radialGradient>
  <filter id="smoke" x="0" y="0" width="100%" height="100%">
    <feTurbulence type="fractalNoise" baseFrequency="0.004 0.011" numOctaves="5" seed="13"/>
    <feColorMatrix type="matrix" values="0 0 0 0 0.12  0 0 0 0 0.03  0 0 0 0 0.02  0 0 0 1.6 -0.55"/>
  </filter>
  <filter id="hotsmoke" x="0" y="0" width="100%" height="100%">
    <feTurbulence type="fractalNoise" baseFrequency="0.006 0.02" numOctaves="4" seed="29"/>
    <feColorMatrix type="matrix" values="0 0 0 0 1  0 0 0 0 0.35  0 0 0 0 0.05  0 0 0 1.3 -0.62"/>
  </filter>
  <filter id="blur3"><feGaussianBlur stdDeviation="3"/></filter>
  <filter id="bigblur" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="60"/></filter>
  <filter id="blur1"><feGaussianBlur stdDeviation="1.2"/></filter>
  <filter id="grain" x="0" y="0" width="100%" height="100%">
    <feTurbulence type="fractalNoise" baseFrequency="0.85" numOctaves="2" seed="3"/>
    <feColorMatrix type="matrix" values="0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  0 0 0 0.18 0"/>
  </filter>
  <radialGradient id="vignette" cx="0.5" cy="0.5" r="0.75">
    <stop offset="0.55" stop-color="#000" stop-opacity="0"/>
    <stop offset="1" stop-color="#000" stop-opacity="0.85"/>
  </radialGradient>
  <linearGradient id="leftshade" x1="0" y1="0" x2="1" y2="0">
    <stop offset="0" stop-color="#000" stop-opacity="0.78"/>
    <stop offset="0.42" stop-color="#000" stop-opacity="0.25"/>
    <stop offset="0.7" stop-color="#000" stop-opacity="0"/>
  </linearGradient>
  <linearGradient id="floor" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#1b0907"/>
    <stop offset="1" stop-color="#050202"/>
  </linearGradient>
"""


def background_main(w=1280, h=720):
    """Hell landscape: red sky, crags, a skull carved in the far mountain, lava."""
    rng = random.Random(1993)
    s = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">' % (w, h, w, h)]
    s.append("<defs>%s%s</defs>" % (BG_DEFS, HELL_DEFS))
    s.append('<rect width="%d" height="%d" fill="url(#hellsky)"/>' % (w, h))
    s.append('<rect width="%d" height="%d" filter="url(#hotsmoke)" opacity="0.5"/>' % (w, h))
    s.append('<rect width="%d" height="%d" filter="url(#smoke)" opacity="0.6"/>' % (w, h))
    # a pentagram burning in the sky behind the mountain
    s.append('<g filter="url(#hugeglow)">%s</g>' % pentagram(w * 0.66, 250, 190, "#ff3a0a", 18, 0.55))
    s.append(pentagram(w * 0.66, 250, 190, "#ff7a2a", 3, 0.35))
    # far crags with the skull carved into the central peak
    s.append('<g opacity="0.9">%s</g>' % crags(rng, w, 470, 210, "#2a0807", 120))
    s.append('<g opacity="0.55">%s</g>' % demon_skull(w * 0.66, 340, 280, eye_glow=False))
    # only the eyes burn at full strength; the skull reads as part of the rock
    for ex in (-0.115, 0.115):
        s.append('<circle cx="%.1f" cy="%.1f" r="22" fill="url(#eyeglow)"/>' % (w * 0.66 + ex * 280, 340 + 0.085 * 280))
    k = 280 / 200.0
    s.append('<path d="%s" fill="#fff" filter="url(#rock)" transform="translate(%.1f,%.1f) scale(%.4f) translate(-100,-100)"/>'
             % (SKULL_CRANIUM, w * 0.66, 340, k))
    s.append('<rect x="0" y="380" width="%d" height="%d" fill="#120303" opacity="0.35" filter="url(#blur3)"/>' % (w, h - 380))
    s.append(crags(rng, w, 560, 150, "#170404", 90))
    # lava river and its glow
    s.append('<ellipse cx="%d" cy="610" rx="620" ry="70" fill="#ff5a12" opacity="0.45" filter="url(#bigblur)"/>' % int(w * 0.6))
    s.append('<path d="M-10,600 C300,575 520,640 820,598 C1000,574 1150,612 %d,596 L%d,640 C1100,660 900,628 640,662 C400,690 200,640 -10,662 Z" fill="url(#lava)"/>' % (w + 10, w + 10))
    s.append(crags(rng, w, 700, 90, "#0b0202", 70))
    s.append('<g filter="url(#blur1)" opacity="0.8">%s</g>' % flames(rng, w, 724, 110, 46, 0.015, 0.04))
    s.append('<g filter="url(#blur1)">%s</g>' % embers(rng, 160, w, h, 120, 700))
    s.append(embers(rng, 60, w, h, 300, 700, 1.4))
    s.append('<rect width="%d" height="%d" fill="url(#leftshade)"/>' % (w, h))
    s.append('<rect width="%d" height="%d" fill="url(#vignette)"/>' % (w, h))
    s.append('<rect width="%d" height="%d" filter="url(#grain)"/>' % (w, h))
    s.append("</svg>")
    return "\n".join(s)


def background_inner(w=1280, h=720):
    """Darker steel bulkhead with hazard trim, for library/settings screens."""
    rng = random.Random(666)
    s = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">' % (w, h, w, h)]
    s.append("<defs>%s" % BG_DEFS)
    s.append("""
  <pattern id="plate" width="160" height="120" patternUnits="userSpaceOnUse">
    <rect width="160" height="120" fill="#140a0a"/>
    <rect x="1" y="1" width="158" height="118" fill="none" stroke="#2a1514" stroke-width="2"/>
    <circle cx="10" cy="10" r="3" fill="#2e1a18"/><circle cx="150" cy="10" r="3" fill="#2e1a18"/>
    <circle cx="10" cy="110" r="3" fill="#2e1a18"/><circle cx="150" cy="110" r="3" fill="#2e1a18"/>
  </pattern>
  <pattern id="hazard" width="40" height="40" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">
    <rect width="20" height="40" fill="#c27a12"/><rect x="20" width="20" height="40" fill="#120707"/>
  </pattern>
  <filter id="metal" x="0" y="0" width="100%" height="100%">
    <feTurbulence type="fractalNoise" baseFrequency="0.02 0.6" numOctaves="3" seed="5"/>
    <feColorMatrix type="matrix" values="0 0 0 0 0.5  0 0 0 0 0.25  0 0 0 0 0.2  0 0 0 0.35 0"/>
  </filter>
  <radialGradient id="lamp" cx="0.5" cy="0" r="0.9">
    <stop offset="0" stop-color="#ff5a1a" stop-opacity="0.55"/>
    <stop offset="1" stop-color="#ff5a1a" stop-opacity="0"/>
  </radialGradient>
</defs>""")
    s.append('<rect width="%d" height="%d" fill="url(#plate)"/>' % (w, h))
    s.append('<rect width="%d" height="%d" filter="url(#metal)"/>' % (w, h))
    s.append('<rect width="%d" height="%d" fill="url(#lamp)"/>' % (w, h))
    s.append('<rect x="0" y="0" width="%d" height="14" fill="url(#hazard)" opacity="0.55"/>' % w)
    s.append('<rect x="0" y="%d" width="%d" height="14" fill="url(#hazard)" opacity="0.55"/>' % (h - 14, w))
    # vertical pipes along the right edge
    for i, x in enumerate((1180, 1214, 1246)):
        s.append('<rect x="%d" y="0" width="22" height="%d" fill="#1e0e0c"/>' % (x, h))
        s.append('<rect x="%d" y="0" width="3" height="%d" fill="#ff7a2a" opacity="0.18"/>' % (x + 3, h))
    s.append(embers(rng, 50, w, h, 0, h, 1.6))
    s.append('<rect width="%d" height="%d" fill="url(#vignette)"/>' % (w, h))
    s.append('<rect width="%d" height="%d" filter="url(#grain)"/>' % (w, h))
    s.append("</svg>")
    return "\n".join(s)



# ---------------------------------------------------------------------------
# hell motifs (original vector art): demon skull, pentagram, flames, crags
# ---------------------------------------------------------------------------

HELL_DEFS = """
  <linearGradient id="bone" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#e8d6bd"/>
    <stop offset="0.45" stop-color="#a8896c"/>
    <stop offset="1" stop-color="#3b2219"/>
  </linearGradient>
  <linearGradient id="horn" x1="0" y1="1" x2="0" y2="0">
    <stop offset="0" stop-color="#2a1210"/>
    <stop offset="0.55" stop-color="#6b2f1c"/>
    <stop offset="1" stop-color="#e2b48a"/>
  </linearGradient>
  <radialGradient id="eyeglow" cx="0.5" cy="0.5" r="0.5">
    <stop offset="0" stop-color="#fff2b0"/>
    <stop offset="0.35" stop-color="#ffb02a"/>
    <stop offset="1" stop-color="#ff2a08" stop-opacity="0"/>
  </radialGradient>
  <linearGradient id="flame" x1="0" y1="1" x2="0" y2="0">
    <stop offset="0" stop-color="#fff0a0"/>
    <stop offset="0.25" stop-color="#ffb02a"/>
    <stop offset="0.6" stop-color="#ff4a10"/>
    <stop offset="1" stop-color="#8a0e04" stop-opacity="0"/>
  </linearGradient>
  <linearGradient id="lava" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#ffcf5a"/>
    <stop offset="0.4" stop-color="#ff5a12"/>
    <stop offset="1" stop-color="#5a0a04"/>
  </linearGradient>
  <radialGradient id="hellsky" cx="0.5" cy="0.95" r="1.0">
    <stop offset="0" stop-color="#ff5a14"/>
    <stop offset="0.22" stop-color="#b0200a"/>
    <stop offset="0.5" stop-color="#3e0806"/>
    <stop offset="1" stop-color="#090203"/>
  </radialGradient>
  <filter id="softglow" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="6"/></filter>
  <filter id="hugeglow" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="24"/></filter>
  <filter id="rock" x="0" y="0" width="100%" height="100%">
    <feTurbulence type="fractalNoise" baseFrequency="0.035 0.08" numOctaves="4" seed="17" result="n"/>
    <feColorMatrix in="n" type="matrix" values="0 0 0 0 0.10  0 0 0 0 0.03  0 0 0 0 0.02  0 0 0 0.9 0" result="c"/>
    <feComposite in="c" in2="SourceGraphic" operator="in"/>
  </filter>
"""

# skull drawn on a 200x200 grid centred on x=100
SKULL_CRANIUM = ("M100,22 C142,22 170,50 170,94 C170,118 160,134 150,144 L148,166 "
                 "C147,176 139,184 128,184 L72,184 C61,184 53,176 52,166 L50,144 "
                 "C40,134 30,118 30,94 C30,50 58,22 100,22 Z")
SKULL_EYE_L = "M56,98 L94,108 L90,130 C77,135 61,128 56,114 Z"
SKULL_EYE_R = "M144,98 L106,108 L110,130 C123,135 139,128 144,114 Z"
SKULL_NOSE = "M100,132 L109,152 L100,147 L91,152 Z"
SKULL_HORN_L = "M52,74 C22,66 4,38 10,0 C22,26 38,40 70,50 Z"
SKULL_HORN_R = "M148,74 C178,66 196,38 190,0 C178,26 162,40 130,50 Z"


def demon_skull(cx, cy, size, eye_glow=True):
    """A horned demon skull centred at (cx, cy), size = width in px."""
    k = size / 200.0
    g = ['<g transform="translate(%.1f,%.1f) scale(%.4f) translate(-100,-100)">' % (cx, cy, k)]
    g.append('<path d="%s" fill="#000" opacity="0.55" transform="translate(6,8)" filter="url(#softglow)"/>' % SKULL_CRANIUM)
    for horn in (SKULL_HORN_L, SKULL_HORN_R):
        g.append('<path d="%s" fill="url(#horn)" stroke="#120505" stroke-width="3"/>' % horn)
    g.append('<path d="%s" fill="url(#bone)" stroke="#140606" stroke-width="4"/>' % SKULL_CRANIUM)
    # cracks and brow ridge
    g.append('<path d="M100,24 L96,46 L104,58 L98,74" fill="none" stroke="#3b2219" stroke-width="2.5"/>')
    g.append('<path d="M50,92 Q100,78 150,92" fill="none" stroke="#5a3424" stroke-width="5" opacity="0.7"/>')
    for eye in (SKULL_EYE_L, SKULL_EYE_R):
        g.append('<path d="%s" fill="#0a0202"/>' % eye)
    if eye_glow:
        for ex in (77, 123):
            g.append('<circle cx="%d" cy="117" r="20" fill="url(#eyeglow)" opacity="0.95"/>' % ex)
            g.append('<circle cx="%d" cy="117" r="5" fill="#fff4c8"/>' % ex)
    g.append('<path d="%s" fill="#0a0202"/>' % SKULL_NOSE)
    # teeth
    g.append('<rect x="66" y="158" width="68" height="24" rx="3" fill="#0a0202"/>')
    for i in range(6):
        x = 68 + i * 11
        g.append('<path d="M%d,158 h9 v%d l-4.5,5 l-4.5,-5 Z" fill="url(#bone)"/>' % (x, 15 if i in (1, 4) else 12))
    g.append("</g>")
    return "\n".join(g)


def pentagram(cx, cy, r, color, width, opacity):
    pts = [(cx + r * math.sin(math.radians(180 + 72 * i)), cy - r * math.cos(math.radians(180 + 72 * i))) for i in range(5)]
    star = " ".join("%.1f,%.1f" % pts[(2 * i) % 5] for i in range(5))
    return ('<g fill="none" stroke="%s" stroke-width="%.1f" opacity="%.2f" stroke-linejoin="miter">'
            '<circle cx="%.1f" cy="%.1f" r="%.1f"/><circle cx="%.1f" cy="%.1f" r="%.1f" stroke-width="%.1f"/>'
            '<polygon points="%s"/></g>' % (color, width, opacity, cx, cy, r * 1.08, cx, cy, r * 1.16, width * 0.5, star))


def flames(rng, w, base_y, height, count, wmin=0.05, wmax=0.12):
    out = []
    for _ in range(count):
        x = rng.uniform(-20, w + 20)
        fw = rng.uniform(wmin, wmax) * w
        fh = rng.uniform(0.45, 1.0) * height
        lean = rng.uniform(-0.25, 0.25) * fw
        out.append('<path d="M%.1f,%.1f Q%.1f,%.1f %.1f,%.1f Q%.1f,%.1f %.1f,%.1f Z" fill="url(#flame)" opacity="%.2f"/>'
                   % (x - fw / 2, base_y, x - fw * 0.55 + lean, base_y - fh * 0.55, x + lean * 1.6, base_y - fh,
                      x + fw * 0.55 + lean, base_y - fh * 0.5, x + fw / 2, base_y, rng.uniform(0.55, 0.95)))
    return "\n".join(out)


def crags(rng, w, base_y, peak, color, step):
    """Jagged hell mountains as one silhouette path."""
    d = ["M-10,%d" % base_y]
    x = -10
    while x < w + 20:
        x += rng.uniform(step * 0.4, step)
        y = base_y - rng.uniform(0.15, 1.0) * peak
        d.append("L%.1f,%.1f" % (x, y))
        x += rng.uniform(step * 0.2, step * 0.6)
        d.append("L%.1f,%.1f" % (x, base_y - rng.uniform(0.0, 0.35) * peak))
    d.append("L%d,%d L-10,%d Z" % (w + 30, base_y + 400, base_y + 400))
    return '<path d="%s" fill="%s"/>' % (" ".join(d), color)


CARD_THEMES = {
    # template id: (base, glow, accent, motif)
    "freedoom2": ("#06120f", "#1fd39a", "#7affd2", "grid"),
    "brutaldoom": ("#160304", "#ff1e1e", "#ff8a5a", "slash"),
    "wadfusion": ("#140b02", "#ff9a1a", "#ffd27a", "rings"),
    "lexicon": ("#0e0616", "#a64dff", "#e0b3ff", "glyphs"),
    "compendium": ("#050b16", "#2d8cff", "#a8d4ff", "stack"),
    "custom": ("#0d0b0b", "#b9a69a", "#f0e6dc", "bolts"),
}


def card_svg(tid, w=480, h=270):
    base, glow, accent, motif = CARD_THEMES[tid]
    rng = random.Random(hash(tid) & 0xffff)
    s = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">' % (w, h, w, h)]
    s.append("""<defs>
  <radialGradient id="g" cx="0.78" cy="0.35" r="0.9">
    <stop offset="0" stop-color="%s" stop-opacity="0.75"/>
    <stop offset="0.45" stop-color="%s" stop-opacity="0.12"/>
    <stop offset="1" stop-color="%s" stop-opacity="0"/>
  </radialGradient>
  <linearGradient id="fade" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0.35" stop-color="#000" stop-opacity="0"/>
    <stop offset="1" stop-color="#000" stop-opacity="0.85"/>
  </linearGradient>
  <filter id="b2"><feGaussianBlur stdDeviation="2"/></filter>
  <filter id="tex" x="0" y="0" width="100%%" height="100%%">
    <feTurbulence type="fractalNoise" baseFrequency="0.012 0.03" numOctaves="4" seed="%d"/>
    <feColorMatrix type="matrix" values="0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  0 0 0 1.2 -0.4"/>
  </filter>
</defs>""" % (glow, glow, glow, rng.randint(1, 99)))
    s.append('<rect width="%d" height="%d" fill="%s"/>' % (w, h, base))
    s.append('<rect width="%d" height="%d" fill="url(#g)"/>' % (w, h))
    s.append('<rect width="%d" height="%d" filter="url(#tex)" opacity="0.7"/>' % (w, h))
    g = []
    if motif == "grid":
        for x in range(0, w, 24):
            g.append('<line x1="%d" y1="0" x2="%d" y2="%d" stroke="%s" stroke-opacity="0.12"/>' % (x, x, h, accent))
        for y in range(0, h, 24):
            g.append('<line x1="0" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-opacity="0.12"/>' % (y, w, y, accent))
        g.append('<path d="M300,40 l60,0 l30,30 l0,90 l-30,30 l-60,0 l-30,-30 l0,-90 z" fill="none" stroke="%s" stroke-width="3" opacity="0.7"/>' % accent)
    elif motif == "slash":
        for i in range(5):
            x = 240 + i * 46
            g.append('<path d="M%d,-10 L%d,-10 L%d,%d L%d,%d Z" fill="%s" opacity="%.2f"/>'
                     % (x, x + 18, x - 60, h + 10, x - 78, h + 10, glow, 0.25 + 0.12 * i))
        for _ in range(40):
            g.append('<circle cx="%.0f" cy="%.0f" r="%.1f" fill="#7a0505" opacity="0.8"/>'
                     % (rng.uniform(200, w), rng.uniform(0, h), rng.uniform(1, 5)))
    elif motif == "rings":
        for r in (40, 70, 100, 130):
            g.append('<circle cx="360" cy="110" r="%d" fill="none" stroke="%s" stroke-width="%d" opacity="%.2f"/>'
                     % (r, accent, 6 if r == 70 else 2, 0.9 - r / 200))
        g.append('<circle cx="360" cy="110" r="22" fill="%s" opacity="0.9" filter="url(#b2)"/>' % glow)
    elif motif == "glyphs":
        for _ in range(26):
            x, y = rng.uniform(220, w - 20), rng.uniform(20, h - 40)
            sz = rng.uniform(8, 22)
            g.append('<path d="M%.0f,%.0f l%.0f,%.0f l%.0f,%.0f m%.0f,0 l0,%.0f" stroke="%s" stroke-width="2" fill="none" opacity="%.2f"/>'
                     % (x, y, sz, -sz, sz, sz, -sz, sz, accent, rng.uniform(0.2, 0.7)))
        g.append('<polygon points="360,30 420,140 300,140" fill="none" stroke="%s" stroke-width="3" opacity="0.8"/>' % glow)
        g.append('<polygon points="360,170 420,60 300,60" fill="none" stroke="%s" stroke-width="3" opacity="0.8"/>' % glow)
    elif motif == "stack":
        for i in range(7):
            g.append('<rect x="%d" y="%d" width="150" height="16" rx="2" fill="none" stroke="%s" stroke-width="2" opacity="%.2f" transform="skewX(-12)"/>'
                     % (330 + i * 4, 36 + i * 22, accent, 0.25 + i * 0.1))
    else:
        for _ in range(14):
            x, y = rng.uniform(220, w - 30), rng.uniform(20, h - 60)
            g.append('<circle cx="%.0f" cy="%.0f" r="9" fill="none" stroke="%s" stroke-width="3" opacity="0.5"/>' % (x, y, accent))
            g.append('<line x1="%.0f" y1="%.0f" x2="%.0f" y2="%.0f" stroke="%s" stroke-width="2" opacity="0.5"/>' % (x - 6, y, x + 6, y, accent))
    s.append("<g>%s</g>" % "\n".join(g))
    s.append('<rect width="%d" height="%d" fill="url(#fade)"/>' % (w, h))
    s.append('<rect x="0" y="0" width="6" height="%d" fill="%s"/>' % (h, glow))
    s.append("</svg>")
    return "\n".join(s)


def icon_svg(size=256):
    """Homebrew menu icon: demon skull and pentagram in hellfire, the steel
    'RDoom' word across it, inside a riveted metal frame."""
    path, w = word_path("RDoom")
    rng = random.Random(666)
    sc = (size * 0.88) / (w + 40)
    u = size / 256.0
    s = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">' % (size, size, size, size)]
    s.append("<defs>%s%s%s</defs>" % (LOGO_DEFS, BG_DEFS, HELL_DEFS))
    s.append('<rect width="%d" height="%d" fill="url(#hellsky)"/>' % (size, size))
    s.append('<rect width="%d" height="%d" filter="url(#hotsmoke)" opacity="0.5"/>' % (size, size))
    c = size / 2
    s.append('<g filter="url(#softglow)">%s</g>' % pentagram(c, size * 0.42, size * 0.36, "#ff3a0a", 7 * u, 0.9))
    s.append(pentagram(c, size * 0.42, size * 0.36, "#ffb04a", 1.6 * u, 0.7))
    s.append(demon_skull(c, size * 0.43, size * 0.62))
    # fire rising in front of the jaw
    s.append('<g filter="url(#blur1)">%s</g>' % flames(rng, size, size * 0.98, size * 0.34, 14))
    s.append('<rect x="0" y="%.1f" width="%d" height="%.1f" fill="#ff6a14" opacity="0.25" filter="url(#softglow)"/>' % (size * 0.86, size, size * 0.14))
    # the word, as in the RDoom logo
    s.append('<g transform="translate(%.1f,%.1f) scale(%.4f) skewX(-10)">' % ((size - w * sc) / 2 + 8 * u, size * 0.66, sc))
    s.append('<path d="%s" fill="#ff4a10" fill-rule="evenodd" filter="url(#glow)"/>' % path)
    for i in range(6, 0, -1):
        s.append('<path d="%s" transform="translate(%g,%g)" fill="rgb(%d,%d,%d)" fill-rule="evenodd"/>' % (path, i * 1.2, i * 1.5, 40 + i * 4, 10, 8))
    s.append('<path d="%s" fill="none" stroke="#050202" stroke-width="16" fill-rule="evenodd"/>' % path)
    s.append('<path d="%s" fill="url(#steel)" fill-rule="evenodd"/>' % path)
    s.append('<path d="%s" fill="none" stroke="url(#rim)" stroke-width="3" fill-rule="evenodd"/>' % path)
    s.append("</g>")
    s.append('<rect width="%d" height="%d" fill="url(#vignette)"/>' % (size, size))
    # riveted frame
    b = 9 * u
    s.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="none" stroke="#2a1512" stroke-width="%.1f"/>' % (b / 2, b / 2, size - b, size - b, b))
    s.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="none" stroke="#ff6a1c" stroke-width="%.1f" opacity="0.8"/>' % (b, b, size - 2 * b, size - 2 * b, 1.4 * u))
    for x in (b / 2, size - b / 2):
        for y in (b / 2, size - b / 2):
            s.append('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="#8a6a5a" stroke="#1a0a08" stroke-width="%.1f"/>' % (x, y, 3.2 * u, 0.8 * u))
    s.append("</svg>")
    return "\n".join(s)



# ---------------------------------------------------------------------------
# Freedoom artwork (BSD licence, see licenses/freedoom-COPYING.txt): the NRO
# icon and the launcher background are built from Freedoom 2's own graphics
# ---------------------------------------------------------------------------

FREEDOOM_WAD = os.path.join(os.path.dirname(ROOT), "third_party", "freedoom", "freedoom-0.13.0", "freedoom2.wad")
FD_DIR = os.path.join(OUT, "freedoom")


def freedoom_pictures(names):
    """Decodes Doom-format pictures from freedoom2.wad into FD_DIR/<name>.png."""
    import struct
    from PIL import Image
    os.makedirs(FD_DIR, exist_ok=True)
    data = open(FREEDOOM_WAD, "rb").read()
    count, diroff = struct.unpack_from("<4xII", data, 0)
    lumps = {}
    for i in range(count):
        off, size, name = struct.unpack_from("<II8s", data, diroff + i * 16)
        lumps.setdefault(name.rstrip(b"\0").decode("ascii", "replace"), (off, size))
    pal = data[lumps["PLAYPAL"][0]:lumps["PLAYPAL"][0] + 768]
    for name in names:
        off, size = lumps[name]
        b = data[off:off + size]
        w, h = struct.unpack_from("<HH", b, 0)
        img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        px = img.load()
        for x in range(w):
            p = struct.unpack_from("<I", b, 8 + 4 * x)[0]
            while b[p] != 0xFF:
                top, length = b[p], b[p + 1]
                for j in range(length):
                    c = b[p + 3 + j]
                    px[x, top + j] = (pal[c * 3], pal[c * 3 + 1], pal[c * 3 + 2], 255)
                p += length + 4
        img.save(os.path.join(FD_DIR, name + ".png"))
    return {n: os.path.join(FD_DIR, n + ".png") for n in names}


def freedoom_icon(pics, logo, out_png):
    """Box-art style: the title screen's red sky (blurred, without its text),
    a flying head demon, the marine firing, and the RDoom word."""
    magick(pics["TITLEPIC"], "-crop", "320x150+0+0", "+repage", "-resize", "1024x1024!", "-blur", "0x18",
           "-modulate", "90,120",
           "(", "-size", "1024x1024", "radial-gradient:#00000000-#000000d0", ")", "-composite",
           "(", pics["HEADA1"], "-filter", "point", "-resize", "820%", ")", "-gravity", "north", "-geometry", "-150+10", "-composite",
           "(", pics["PLAYF1"], "-filter", "point", "-resize", "1350%", ")", "-gravity", "south", "-geometry", "+130+70", "-composite",
           "(", logo, "-resize", "1000x", ")", "-gravity", "south", "-geometry", "+0+4", "-composite",
           "-gravity", "center", "-bordercolor", "#1a0b08", "-shave", "14x14", "-border", "14x14",
           out_png)


def freedoom_background(pics, out_png, w=1280, h=720):
    """The giant burning skull of Freedoom's BOSSBACK, darker on the left where
    the launcher draws its menus."""
    magick(pics["BOSSBACK"], "-modulate", "118,110", "-filter", "point", "-resize", "200%", "-filter", "Lanczos", "-resize", "%dx" % w,
           "-gravity", "center", "-crop", "%dx%d+0+0" % (w, h), "+repage",
           "(", "-size", "%dx%d" % (h, w), "gradient:#00000090-#00000000", "-rotate", "-90", ")", "-composite",
           "(", "-size", "%dx%d" % (w, h), "radial-gradient:#00000000-#00000070", ")", "-composite",
           out_png)

# ---------------------------------------------------------------------------

def render(svg_text, name, w=None, h=None):
    os.makedirs(SVG_DIR, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)
    svg_path = os.path.join(SVG_DIR, name + ".svg")
    with open(svg_path, "w") as f:
        f.write(svg_text)
    png = os.path.join(OUT, name + ".png")
    cmd = ["rsvg-convert", "-o", png]
    if w:
        cmd += ["-w", str(w)]
    if h:
        cmd += ["-h", str(h)]
    subprocess.run(cmd + [svg_path], check=True)
    return png


def magick(*args):
    subprocess.run(["magick"] + list(args), check=True)


def main():
    logo = render(logo_svg(1400, 420), "rdoom_logo")
    render(logo_svg(1400, 420, with_glow=False), "rdoom_logo_flat")
    magick(logo, "-resize", "620x", os.path.join(OUT, "logo_620.png"))
    magick(logo, "-resize", "300x", os.path.join(OUT, "logo_300.png"))
    magick(logo, "-resize", "400x", os.path.join(OUT, "logo_400.png"))
    plate = render(logo_svg(1400, 420, plate=True), "rdoom_logo_plate")
    magick(plate, "-resize", "1200x", "-quality", "90", os.path.join(OUT, "rdoom_logo_dark.jpg"))

    pics = freedoom_pictures(["TITLEPIC", "BOSSBACK", "HEADA1", "PLAYF1"])
    bg = os.path.join(OUT, "bg_main.png")
    freedoom_background(pics, bg)
    magick(bg, "-quality", "88", "-sampling-factor", "4:2:0", os.path.join(OUT, "bg_main.jpg"))
    inner = render(background_inner(), "bg_inner")
    magick(inner, "-quality", "86", "-sampling-factor", "4:2:0", os.path.join(OUT, "bg_inner.jpg"))

    for tid in CARD_THEMES:
        c = render(card_svg(tid), "card_" + tid)
        magick(c, "-quality", "88", os.path.join(OUT, "card_%s.jpg" % tid))

    ic = os.path.join(OUT, "icon_1024.png")
    freedoom_icon(pics, logo, ic)
    # the NRO icon must be a 256x256 baseline JPEG
    magick(ic, "-resize", "256x256", "-quality", "92", "-interlace", "none", os.path.join(OUT, "icon_256.jpg"))
    magick(ic, "-resize", "512x512", os.path.join(OUT, "icon_512.png"))

    # splash for the loading screen (logo over the main background)
    magick(os.path.join(OUT, "bg_main.png"), os.path.join(OUT, "logo_620.png"), "-gravity", "center",
           "-geometry", "+0-40", "-composite", "-quality", "88", os.path.join(OUT, "splash.jpg"))

    # social/documentation thumbnail
    magick(os.path.join(OUT, "bg_main.png"), os.path.join(OUT, "logo_620.png"), "-gravity", "west",
           "-geometry", "+60-60", "-composite", "-resize", "1280x720", "-quality", "88",
           os.path.join(OUT, "promo_1280x720.jpg"))
    print("artwork written to", OUT)


if __name__ == "__main__":
    sys.exit(main())
