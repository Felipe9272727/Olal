# Olal OS — no seu celular (Android, sem root)

O Olal OS roda no seu Android usando o **Termux** (um Linux real no celular).
A interface é a mesma do Olal — home com os apps, a **Olal AI** (que conversa e
controla o sistema), **YouTube** real, navegador e touch.

> ⚡ **Quer ver agora, sem instalar nada?** Abra no Chrome do celular:
> **https://felipe9272727.github.io/Olal/olal-os/shell/**

## Por que duas formas de abrir?
Renderizar o Chromium do Debian/proot no Termux:X11 dá **tela preta** em muitos
celulares arm64 (bug do SwiftShader do Chromium no ARM). Por isso o Olal tem
**dois modos**, e o recomendado renderiza 100%.

## Instalação
1. Instale o **Termux** pelo **F-Droid** (não pela Play Store).
2. (Opcional, só para o modo tela cheia) instale o app **Termux:X11** (F-Droid).
3. No Termux:
```bash
pkg install -y git
git clone https://github.com/Felipe9272727/Olal
cd Olal/olal-os
bash install.sh
```

## Abrir o Olal

### ✅ Recomendado — abre no navegador do celular (renderiza 100%)
```bash
cd ~
./olal-web
```
Abre o Olal no Chrome do seu celular, com YouTube, IA e touch funcionando.
**Dica:** no Chrome, menu (⋮) → **"Adicionar à tela inicial"** para abrir o
Olal em tela cheia, como um app de verdade.

### 🖥️ Tela cheia no Termux:X11 (Chromium nativo do Termux)
```bash
cd ~
./olal
```
Depois abra o app **Termux:X11**. Usa o Chromium **nativo do Termux** (do
`tur-repo`), que renderiza no Termux:X11 — diferente do Chromium do proot.
Se mesmo assim ficar preto no seu aparelho, use o `./olal-web` acima.

## O que funciona
- **Olal AI (o coração da OS)** — conversa (LLM real) e **controla o sistema**:
  "toca lofi no youtube", "abre a calculadora", "quanto é 17×23". Os comandos
  funcionam **mesmo sem internet**.
- **YouTube** — busca real e toca vídeo de verdade.
- **Navegador, Terminal, Calc, Notas, Relógio, Paint, OLA-32 (nossa CPU),
  Sistema, Rede, Config, Arquivos, Olal JS** — todos com o mesmo design e touch.
