#!/usr/bin/env python3
"""Replace every CHKFEAT X16 (HINT #40, 0xd503251f) in executable sections
with NOP (0xd503201f), and the Guarded Control Stack instructions that only
run when CHKFEAT reports GCS (mrs xN, gcspr_el0 -> mov xN, xzr; gcspopm -> nop).
Ryujinx refuses to translate a block containing GCS instructions even when
they would never execute.

Why: GCC 15's libgcc unwinder asks "is the Guarded Control Stack enabled?"
with `mov x16, #1; chkfeat x16` and reads GCSPR_EL0 only when X16 comes back
0. CHKFEAT is in the HINT space, so on CPUs without FEAT_CHK -- the Switch's
Cortex-A57 -- it executes as a NOP: X16 stays 1 and GCSPR_EL0 is never read.
Replacing it with NOP is therefore exactly what the console does. Ryujinx
1.3.x does not treat HINT #40 as a NOP, reaches `mrs x1, gcspr_el0` during
the first C++ exception and aborts ("Unknown MRS 0xD53B2521").

    patch_chkfeat.py <elf> [--expect-min N]
"""
import struct
import sys

CHKFEAT = 0xD503251F
NOP = 0xD503201F
MRS_GCSPR_MASK, MRS_GCSPR = 0xFFFFFFE0, 0xD53B2520   # mrs xN, gcspr_el0
MOV_XZR = 0xAA1F03E0                                 # mov xN, xzr (orr xN, xzr, xzr)
GCSPOPM_MASK, GCSPOPM = 0xFFFFFFE0, 0xD52B7720       # gcspopm (sys #3, c7, c7, #1, xN)
SHF_EXECINSTR = 0x4


def main():
    path = sys.argv[1]
    data = bytearray(open(path, "rb").read())
    if data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
        sys.exit("%s: not a little-endian ELF64 file" % path)
    shoff = struct.unpack_from("<Q", data, 0x28)[0]
    shentsize, shnum = struct.unpack_from("<HH", data, 0x3A)
    patched = 0
    gcs = 0
    for i in range(shnum):
        off = shoff + i * shentsize
        sh_type, sh_flags = struct.unpack_from("<IQ", data, off + 4)
        sh_offset, sh_size = struct.unpack_from("<QQ", data, off + 0x18)
        if sh_type != 1 or not (sh_flags & SHF_EXECINSTR):  # SHT_PROGBITS + exec
            continue
        for p in range(sh_offset, sh_offset + sh_size - 3, 4):
            w = struct.unpack_from("<I", data, p)[0]
            if w == CHKFEAT:
                struct.pack_into("<I", data, p, NOP)
                patched += 1
            elif (w & MRS_GCSPR_MASK) == MRS_GCSPR:
                # only reached when CHKFEAT reported GCS enabled: dead code here
                struct.pack_into("<I", data, p, MOV_XZR | (w & 0x1F))
                gcs += 1
            elif (w & GCSPOPM_MASK) == GCSPOPM:
                struct.pack_into("<I", data, p, NOP)
                gcs += 1
    open(path, "wb").write(data)
    print("%s: %d CHKFEAT X16 -> NOP, %d GCS instruction(s) behind them neutralised" % (path, patched, gcs))
    if "--expect-min" in sys.argv and patched < int(sys.argv[sys.argv.index("--expect-min") + 1]):
        sys.exit("fewer CHKFEAT instructions than expected")


if __name__ == "__main__":
    main()
