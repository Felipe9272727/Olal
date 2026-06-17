# Olal OS — distro Debian de verdade no seu celular (Android, sem root)

O Olal OS roda um **Debian Linux real com desktop XFCE** no seu Android (via
Termux + Termux:X11), com **aceleração de GPU (virgl)** para o navegador
renderizar, e a **cara do Olal** por cima (wallpaper, apps, a Olal AI, OLA-32).

> ⚡ **Só quer a interface, sem instalar nada?** Abra no Chrome do celular:
> **https://felipe9272727.github.io/Olal/olal-os/shell/**

## Por que dava tela preta (e a correção verídica)
A tela preta era **específica do Chromium**: o renderizador por software
(SwiftShader) **não pinta no ARM**. Duas soluções, ambas usadas aqui:
- O **desktop XFCE** e o **Firefox** renderizam pelo caminho **GTK/X11 + virgl**
  (o mesmo do desktop — se o desktop aparece, o Firefox aparece).
- Para GPU **acelerada de verdade** na **Adreno** (Snapdragon), o Chromium roda
  via **zink = Turnip** (Vulkan real da Adreno): `MESA_LOADER_DRIVER_OVERRIDE=zink`.
  (Método comprovado pelo **phoenixbyrd/Termux_XFCE**, testado em Adreno.)

## Instalação — caminho recomendado (máxima certeza, p/ Adreno)
Usa a base comprovada em Adreno (XFCE + Turnip) e põe o Olal por cima:
```bash
# 1) base testada em Adreno (XFCE + GPU Turnip + navegador acelerado)
curl -sL https://raw.githubusercontent.com/phoenixbyrd/Termux_XFCE/main/install_xfce_native.sh -o inst.sh && bash inst.sh
# 2) adiciona a cara + apps do Olal
curl -sL https://raw.githubusercontent.com/Felipe9272727/Olal/main/olal-os/olal-layer.sh -o olal-layer.sh && bash olal-layer.sh
```
No desktop, abra o app **"Olal AI"**.

## Instalação — Olal self-contained
1. Instale **Termux** e **Termux:X11** pelo **F-Droid** (não pela Play Store).
2. No Termux:
```bash
pkg install -y git
git clone https://github.com/Felipe9272727/Olal
cd Olal/olal-os
bash install.sh
```
Baixa Debian + XFCE + Firefox + Chromium + GPU (demora; ~1.5 GB).

## Abrir o desktop do Olal
```bash
cd ~
./olal
```
Depois **abra o app Termux:X11** → o desktop XFCE do Olal aparece em tela cheia.

### Confirme a GPU (importante!)
No desktop, abra o **Terminal** (ou o atalho "Testar GPU") e rode:
```bash
gpu-test
```
Tem que aparecer **`virgl`** (não `llvmpipe`). Se aparecer virgl, o **Firefox
e o Chromium renderizam**. Abra o **Firefox-ESR** ou o atalho **"Olal AI"**.

> Se o navegador ainda ficar preto: seu chip pode precisar do caminho
> zink+turnip (só Qualcomm/Adreno) — me avise o modelo do aparelho.

## Modo garantido (sem desktop)
Se quiser só a interface do Olal, sem o desktop XFCE:
```bash
cd ~ && ./olal-web
```
Abre o Olal no Chrome do próprio celular (renderiza 100%).

## O que tem na distro
- **Desktop XFCE real** (gerenciador de janelas, terminal, arquivos).
- **Olal AI** — assistente que conversa (LLM) e controla o sistema.
- **OLA-32** — nossa CPU própria (`ola32` no terminal).
- **Firefox** acelerado por GPU, com YouTube.
- Wallpaper e identidade do Olal.
