#!/usr/bin/env python3
"""Treina uma rede neural char-level (MLP com janela de contexto) e exporta
   os pesos para kernel/model.h, para a IA rodar dentro do kernel do Olal OS."""
import numpy as np, unicodedata, sys

CTX = 16          # janela de contexto
EMB = 18          # embedding
HID = 256         # camada escondida
EPOCHS = 6000
LR = 0.02
np.random.seed(1)

def deaccent(s):
    s = unicodedata.normalize("NFKD", s)
    return "".join(c for c in s if not unicodedata.combining(c))

text = open("tools/corpus.txt").read()
text = deaccent(text).lower()
text = "".join(c for c in text if c in "abcdefghijklmnopqrstuvwxyz0123456789 .,!?:\n")
text = text.replace("\n", " ")
while "  " in text: text = text.replace("  ", " ")

chars = sorted(set(text))
V = len(chars)
stoi = {c:i for i,c in enumerate(chars)}
itos = {i:c for c,i in stoi.items()}
print(f"corpus={len(text)} chars, vocab={V}: {''.join(chars)!r}")

data = [stoi[c] for c in text]
X, Y = [], []
for i in range(CTX, len(data)):
    X.append(data[i-CTX:i]); Y.append(data[i])
X = np.array(X); Y = np.array(Y)
N = len(X)
print(f"amostras={N}, params~{V*EMB + CTX*EMB*HID + HID + HID*V + V}")

# parametros
Emb = np.random.randn(V, EMB) * 0.1
W1 = np.random.randn(CTX*EMB, HID) * (1.0/np.sqrt(CTX*EMB))
b1 = np.zeros(HID)
W2 = np.random.randn(HID, V) * (1.0/np.sqrt(HID))
b2 = np.zeros(V)

# Adam
params = [Emb, W1, b1, W2, b2]
m = [np.zeros_like(p) for p in params]
v = [np.zeros_like(p) for p in params]
b1a, b2a, eps = 0.9, 0.999, 1e-8

def forward(xb):
    e = Emb[xb].reshape(len(xb), CTX*EMB)   # (B, CTX*EMB)
    h = np.tanh(e @ W1 + b1)                 # (B, HID)
    logits = h @ W2 + b2                      # (B, V)
    return e, h, logits

t = 0
BS = 256
for epoch in range(EPOCHS):
    idx = np.random.randint(0, N, BS)
    xb, yb = X[idx], Y[idx]
    e, h, logits = forward(xb)
    logits -= logits.max(1, keepdims=True)
    p = np.exp(logits); p /= p.sum(1, keepdims=True)
    loss = -np.log(p[np.arange(BS), yb] + 1e-9).mean()

    dlogits = p.copy(); dlogits[np.arange(BS), yb] -= 1; dlogits /= BS
    dW2 = h.T @ dlogits; db2 = dlogits.sum(0)
    dh = dlogits @ W2.T * (1 - h*h)
    dW1 = e.T @ dh; db1 = dh.sum(0)
    de = (dh @ W1.T).reshape(BS, CTX, EMB)
    dEmb = np.zeros_like(Emb)
    for i in range(BS):
        for j in range(CTX):
            dEmb[xb[i, j]] += de[i, j]

    grads = [dEmb, dW1, db1, dW2, db2]
    t += 1
    for k in range(len(params)):
        m[k] = b1a*m[k] + (1-b1a)*grads[k]
        v[k] = b2a*v[k] + (1-b2a)*grads[k]**2
        mh = m[k]/(1-b1a**t); vh = v[k]/(1-b2a**t)
        params[k] -= LR * mh/(np.sqrt(vh)+eps)
    if epoch % 400 == 0:
        print(f"epoch {epoch:5d}  loss {loss:.3f}")

# amostra de teste
def sample(seed=" ", n=200, temp=0.7):
    ctx = [stoi.get(c,0) for c in (" "*CTX + seed)][-CTX:]
    out = ""
    for _ in range(n):
        _, _, lg = forward(np.array([ctx]))
        lg = lg[0]/temp; lg -= lg.max()
        p = np.exp(lg); p /= p.sum()
        i = np.random.choice(V, p=p)
        out += itos[i]; ctx = ctx[1:] + [i]
    return out
print("\n--- amostra ---")
print(sample("oi ", 200))
print(sample("o sistema ", 200))

# exporta para C
def arr(name, a):
    flat = a.astype(np.float32).flatten()
    s = f"static const float {name}[{len(flat)}] = {{\n"
    s += ",".join(f"{x:.6f}" for x in flat)
    return s + "\n};\n"

with open("kernel/model.h", "w") as f:
    f.write("/* Modelo de IA char-level gerado por tools/train.py */\n")
    f.write("#ifndef MODEL_H\n#define MODEL_H\n")
    f.write(f"#define M_CTX {CTX}\n#define M_EMB {EMB}\n#define M_HID {HID}\n#define M_V {V}\n")
    f.write(f'static const char M_VOCAB[] = "' + "".join(
        c if c != '"' else '\\"' for c in chars) + '";\n')
    f.write(arr("M_EMBW", Emb))
    f.write(arr("M_W1", W1))
    f.write(arr("M_B1", b1))
    f.write(arr("M_W2", W2))
    f.write(arr("M_B2", b2))
    f.write("#endif\n")
print("\nkernel/model.h exportado")
