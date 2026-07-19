#!/usr/bin/env python3
# M47 diagnostic: diff a captured FDC write-trace (fdcwrite.log) sector against
# the FM-PAC .sram reference, report the delta distribution + contiguous corrupt
# regions. Revealed the systematic constant -0x8A offset (keyed-save divergence).
# Usage: python tools/analyze/save-corruption-diff.py <fdcwrite.log> <lba> <fmpac.rom.sram>
import re,sys,os
from collections import Counter
def main(logp,lba,sramp):
    lba=int(lba); w={}
    for line in open(logp):
        m=re.search(r"lba=(\d+).*idx=(\d+) val=([0-9A-Fa-f]{2})",line)
        if m: w.setdefault(int(m.group(1)),{})[int(m.group(2))]=int(m.group(3),16)
    if lba not in w: sys.exit(f"lba {lba} not in log; present: {sorted(w)}")
    buf=bytes(w[lba][i] for i in range(512))
    ref=open(sramp,"rb").read()[16:16+512]     # skip 16-byte PAC2 header
    diff=[i for i in range(512) if buf[i]!=ref[i]]
    dh=Counter(((ref[i]-buf[i])&0xFF) for i in diff)
    print(f"match {512-len(diff)}/512  differ {len(diff)}/512")
    print("top deltas (correct-corrupt mod256):",dh.most_common(6))
    reg=[]; 
    if diff:
        s=p=diff[0]
        for i in diff[1:]:
            if i==p+1:p=i
            else:reg.append((s,p));s=p=i
        reg.append((s,p))
    print("corrupt regions:",[f"[{a}..{b}]({b-a+1})" for a,b in reg])
if __name__=="__main__":
    if len(sys.argv)!=4: sys.exit(__doc__ if __doc__ else "args: log lba sram")
    main(*sys.argv[1:])
