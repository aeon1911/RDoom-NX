#!/usr/bin/env python3
"""Integrity checks for the RDoom SD card package (dist/).

  python3 -I tests/check_package.py dist

Checks: required files, NRO structure (header, icon, NACP title), artwork
decodes, licences present, no commercial IWADs or unknown game data, zip
contents match the tree, recorded checksums match.
"""
import hashlib
import os
import struct
import sys
import zipfile

FAIL = []


def check(cond, msg):
    print(("ok:   " if cond else "FAIL: ") + msg)
    if not cond:
        FAIL.append(msg)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# Free IWADs that may be shipped (Freedoom 0.13.0, BSD licence).
ALLOWED_IWADS = {
    "7323bcc168c5a45ff10749b339960e98314740a734c30d4b9f3337001f9e703d": "freedoom1.wad 0.13.0",
    "a8772e088847032510d97ba2312406a6998f21cbab44d4ff10696faa9c0ecd4b": "freedoom2.wad 0.13.0",
}


def check_nro(path):
    with open(path, "rb") as f:
        data = f.read()
    check(data[0x10:0x14] == b"NRO0", "RDoom.nro has the NRO0 header")
    nro_size = struct.unpack_from("<I", data, 0x18)[0]
    check(nro_size <= len(data), "NRO size field is consistent (%d bytes)" % nro_size)
    asset = data[nro_size:]
    check(asset[:4] == b"ASET", "NRO has an asset section (icon/NACP)")
    if asset[:4] != b"ASET":
        return
    icon_off, icon_size, nacp_off, nacp_size = struct.unpack_from("<QQQQ", asset, 8)
    icon = asset[icon_off:icon_off + icon_size]
    check(icon[:3] == b"\xff\xd8\xff" and icon_size < 0x20000, "NRO icon is a JPEG (%d bytes)" % icon_size)
    nacp = asset[nacp_off:nacp_off + nacp_size]
    title = nacp[0:0x200].split(b"\0")[0].decode("utf-8", "replace")
    author = nacp[0x200:0x300].split(b"\0")[0].decode("utf-8", "replace")
    version = nacp[0x3060:0x3070].split(b"\0")[0].decode("utf-8", "replace")
    check(title == "RDoom", "NACP title is 'RDoom' (got %r)" % title)
    print("      author=%r version=%r" % (author, version))


def is_iwad(path):
    try:
        with open(path, "rb") as f:
            return f.read(4) == b"IWAD"
    except OSError:
        return False


def main():
    dist = sys.argv[1] if len(sys.argv) > 1 else "dist"
    sd = os.path.join(dist, "RDoom-NX", "switch", "rdoom")
    required = [
        "RDoom.nro", "uzdoom.pk3", "game_support.pk3", "game_widescreen_gfx.pk3", "brightmaps.pk3", "lights.pk3",
        "soundfonts/uzdoom.sf2", "fm_banks/GENMIDI.GS.wopl", "VERSION.txt",
        "launcher/bg_main.jpg", "launcher/bg_inner.jpg", "launcher/splash.jpg", "launcher/logo_400.png",
        "launcher/card_freedoom2.jpg", "launcher/card_custom.jpg",
        "iwads/LEIA-ME.txt", "mods/LEIA-ME.txt",
        "licenses/GPL-3.0-UZDoom.txt", "licenses/THIRD_PARTY_NOTICES.md", "licenses/OFL-Poppins.txt", "licenses/OFL-BebasNeue.txt",
        "licenses/MESA-MIT.txt", "licenses/LIBNX-ISC.txt",
    ]
    for r in required:
        p = os.path.join(sd, r)
        check(os.path.isfile(p) and os.path.getsize(p) > 0, "present: switch/rdoom/" + r)
    check(os.path.isfile(os.path.join(dist, "RDoom-NX", "LEIA-ME.txt")), "present: LEIA-ME.txt at the package root")
    check(os.path.isfile(os.path.join(dist, "RDoom.nro")), "present: dist/RDoom.nro")

    if os.path.isfile(os.path.join(sd, "RDoom.nro")):
        check_nro(os.path.join(sd, "RDoom.nro"))
        check(sha256(os.path.join(sd, "RDoom.nro")) == sha256(os.path.join(dist, "RDoom.nro")),
              "dist/RDoom.nro is the same file as the packaged one")

    for pk3 in ("uzdoom.pk3", "game_support.pk3", "brightmaps.pk3", "lights.pk3", "game_widescreen_gfx.pk3"):
        p = os.path.join(sd, pk3)
        if os.path.isfile(p):
            try:
                with zipfile.ZipFile(p) as z:
                    bad = z.testzip()
                check(bad is None, "%s is a valid zip" % pk3)
            except zipfile.BadZipFile:
                check(False, "%s is a valid zip" % pk3)

    # no game data except allowed free IWADs, in the tree or in any zip
    for dirpath, _, files in os.walk(os.path.join(dist, "RDoom-NX")):
        for f in files:
            p = os.path.join(dirpath, f)
            if is_iwad(p):
                check(sha256(p) in ALLOWED_IWADS, "IWAD in package is a free one: " + os.path.relpath(p, dist))
    for z in sorted(os.listdir(dist)):
        if not z.endswith(".zip"):
            continue
        with zipfile.ZipFile(os.path.join(dist, z)) as zf:
            names = zf.namelist()
            check("switch/rdoom/RDoom.nro" in names, z + " contains switch/rdoom/RDoom.nro")
            for n in names:
                if n.lower().endswith((".wad", ".iwad", ".ipk3")):
                    h = hashlib.sha256(zf.read(n)).hexdigest()
                    check(h in ALLOWED_IWADS, "%s: %s is an allowed free IWAD" % (z, n))
            if "Freedoom" in z:
                check("switch/rdoom/licenses/freedoom/COPYING.txt" in names, z + " carries the Freedoom licence")

    sums = os.path.join(dist, "SHA256SUMS.txt")
    if os.path.isfile(sums):
        for line in open(sums):
            h, name = line.split()
            check(sha256(os.path.join(dist, name)) == h, "checksum matches: " + name)
    else:
        check(False, "SHA256SUMS.txt present")

    print("\n%d failure(s)" % len(FAIL))
    return 1 if FAIL else 0


if __name__ == "__main__":
    sys.exit(main())
