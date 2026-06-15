#!/usr/bin/env python3
"""Montador da arquitetura OLA-32 (criada para o Olal OS).
   Uso: python3 arch/oasm.py prog.s -o prog.bin [--c-array nome]
   Gera codigo de maquina (32 bits por instrucao, little-endian)."""
import sys, re

R = {f"r{i}": i for i in range(16)}
OPS = {
    "nop":(0x00,"N"), "add":(0x01,"R"), "sub":(0x02,"R"), "and":(0x03,"R"),
    "or":(0x04,"R"), "xor":(0x05,"R"), "shl":(0x06,"R"), "shr":(0x07,"R"),
    "slt":(0x08,"R"), "mul":(0x09,"R"),
    "addi":(0x10,"I"), "li":(0x11,"LI"), "lw":(0x12,"M"), "sw":(0x13,"M"),
    "beq":(0x14,"B"), "bne":(0x15,"B"), "jmp":(0x16,"J"), "jal":(0x17,"J"),
    "jr":(0x18,"JR"), "sys":(0x1F,"SYS"), "hlt":(0x3F,"N"),
}

def reg(t):
    t = t.strip().lower()
    if t not in R: raise SystemExit(f"registrador invalido: {t}")
    return R[t]

def imm(t, labels=None, pc=None, rel=False):
    t = t.strip()
    if labels is not None and t in labels:
        return labels[t] - (pc + 1) if rel else labels[t]
    return int(t, 0)

def enc_r(op,rd,rs,rt): return (op<<26)|(rd<<22)|(rs<<18)|(rt<<14)
def enc_i(op,rd,rs,im): return (op<<26)|(rd<<22)|(rs<<18)|(im & 0x3FFFF)
def enc_j(op,ad): return (op<<26)|(ad & 0x3FFFFFF)

def tokenize(line):
    line = line.split(";")[0].strip()
    return line

def assemble(src):
    # passo 1: coleta labels e instrucoes
    insns = []   # (mnem, args, srcline)
    labels = {}
    for raw in src.splitlines():
        line = tokenize(raw)
        if not line: continue
        while ":" in line:
            lab, _, rest = line.partition(":")
            labels[lab.strip()] = len(insns)
            line = rest.strip()
            if not line: break
        if not line: continue
        parts = re.split(r"[ \t]+", line, maxsplit=1)
        mnem = parts[0].lower()
        args = [a.strip() for a in parts[1].split(",")] if len(parts) > 1 else []
        insns.append((mnem, args))
    # passo 2: codifica
    out = []
    for pc, (mnem, a) in enumerate(insns):
        if mnem not in OPS: raise SystemExit(f"instrucao desconhecida: {mnem}")
        op, kind = OPS[mnem]
        if kind == "N":
            out.append(enc_r(op,0,0,0))
        elif kind == "R":      # rd, rs, rt
            out.append(enc_r(op, reg(a[0]), reg(a[1]), reg(a[2])))
        elif kind == "I":      # rd, rs, imm   (addi)
            out.append(enc_i(op, reg(a[0]), reg(a[1]), imm(a[2], labels, pc)))
        elif kind == "LI":     # rd, imm
            out.append(enc_i(op, reg(a[0]), 0, imm(a[1], labels, pc)))
        elif kind == "M":      # rd, rs, imm   (lw/sw: mem[rs+imm])
            out.append(enc_i(op, reg(a[0]), reg(a[1]), imm(a[2], labels, pc)))
        elif kind == "B":      # rd, rs, label  (relativo)
            out.append(enc_i(op, reg(a[0]), reg(a[1]), imm(a[2], labels, pc, rel=True)))
        elif kind == "J":      # label/addr
            out.append(enc_j(op, imm(a[0], labels, pc)))
        elif kind == "JR":     # rs
            out.append(enc_r(op, 0, reg(a[0]), 0))
        elif kind == "SYS":    # rs, code
            out.append(enc_i(op, 0, reg(a[0]), imm(a[1], labels, pc)))
    return out

def main():
    args = sys.argv[1:]
    src_path = args[0]
    out_path = "a.bin"; carr = None
    i = 1
    while i < len(args):
        if args[i] == "-o": out_path = args[i+1]; i += 2
        elif args[i] == "--c-array": carr = args[i+1]; i += 2
        else: i += 1
    words = assemble(open(src_path).read())
    with open(out_path, "wb") as f:
        for w in words:
            f.write(bytes([w & 255, (w>>8)&255, (w>>16)&255, (w>>24)&255]))
    print(f"{src_path}: {len(words)} instrucoes -> {out_path}")
    if carr:
        with open(carr, "w") as f:
            f.write("/* programa OLA-32 gerado por arch/oasm.py */\n")
            f.write(f"static const unsigned int {carr.split('/')[-1].split('.')[0]}[] = {{\n")
            f.write(",".join(f"0x{w:08X}" for w in words))
            f.write("\n};\n")
        print(f"  array C -> {carr}")

if __name__ == "__main__":
    main()
