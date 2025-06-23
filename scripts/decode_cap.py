#!/usr/bin/env python3
import sys

cap_hex = sys.argv[1]

# Assume the argument is of the form 'addr+metadata' or a single 64-bit hex
# word with address in least significant bits.
if '+' in cap_hex:
    (addr_hex, meta_hex) = cap_hex.split('+')
else:
    addr_hex=cap_hex[-8:]
    meta_hex=cap_hex[:-8]
addr=int(addr_hex, 16)
meta=int(meta_hex, 16)
B=meta & 0x1ff
T=(meta >> 9) & 0x1ff
E=(meta >> 18) & 0xf
otype = (meta >> 22) & 0x7
perms = (meta >> 25) & 0x3f
R=(meta >> 31) & 0x1
v=(meta >> 32) & 0x1
print(f'raw: v={v} R={R} p={perms:6b} otype={otype} E={E} T=0x{T:03x} B=0x{B:03x} addr=0x{addr:08x}')
if R != 0:
    print("Reserved bit set!")

GL=(perms >> 5) & 1
LD,SD,MC,SL,LM,LG,SR,EX,SR,U0,SE,US,=12*(0,)
p4 = (perms >> 4) & 1
p3 = (perms >> 3) & 1
p2 = (perms >> 2) & 1
p1 = (perms >> 1) & 1
p0 = (perms >> 0) & 1

if p4 and p3:
    LD, MC, SD=1, 1, 1
    SL, LM, LG=p2, p1, p0
elif p4 and not p3 and p2:
    LD, MC = 1, 1
    LM, LG = p1, p0
elif p4 and not p3 and not p2 and not p1 and not p0:
    SD, MC = 1, 1
elif p4 and not p3 and not p2:
    LD, SD = p1, p0
elif not p4 and p3:
    EX, LD, MC = 1, 1, 1
    SR, LM, LG = p2, p1, p0
elif not p4 and not 3:
    U0, SE, US = p2, p1, p0

def pc(p,c):
    return c if p else '-'
perm_str=pc(GL, 'G')
perm_str+=' '
perm_str+=pc(LD, 'R')
perm_str+=pc(SD, 'W')
perm_str+=pc(MC, 'c')
perm_str+=pc(LG, 'g')
perm_str+=pc(LM, 'm')
perm_str+=' '
perm_str+=pc(EX, 'X')
perm_str+=pc(SR, 'a')
perm_str+=' '
perm_str+=pc(SE, 'S')
perm_str+=pc(US, 'U')
perm_str+=pc(U0, '0')

if otype!=0 and not EX:
    otype += 8
e=E if E != 15 else 24

a_top=addr >> (e+9)
a_mid=(addr >> e) & 0x1ff
a_hi=1 if a_mid < B else 0
t_hi=1 if T < B else 0
c_b=-a_hi
c_t=t_hi-a_hi
a_top_base=a_top + c_b
a_top_top =a_top + c_t
base = ((a_top_base << 9) + B) << e
top = ((a_top_top << 9) + T) << e
print(f'{base:08x}-{top:09x} l:{top-base:09x}')

otypes={
    0:'unsealed',
    1:'IRQ inherit forward sentry',
    2:'IRQ disable forward sentry',
    3:'IRQ enable forward sentry',
    4:'IRQ disable return sentry',
    5:'IRQ enable return sentry',
}
otype_str=str(otype) + (f' ({otypes[otype]})' if otype in otypes else '')

print(f'otype={otype_str}')
print(perm_str)
