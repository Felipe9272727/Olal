#!/data/data/com.termux/files/usr/bin/bash
# ============================================================
#  Olal OS - instalador (Android/Termux, SEM root).
#  Monta uma DISTRO de verdade: Debian + desktop XFCE no
#  Termux:X11, com aceleracao de GPU (virgl) e a cara do Olal.
#  Inclui tambem o modo "olal-web" (a prova de balas).
# ============================================================
set -e
SCRIPTDIR="$(cd "$(dirname "$0")" && pwd)"

echo "==================================================="
echo "   Olal OS  -  distro Debian no seu celular"
echo "==================================================="

echo ">> [1/4] Termux: proot, X11, audio, GPU (Turnip/Adreno)..."
yes | pkg update || true
# x11-repo (Termux:X11/virgl) + tur-repo (driver Turnip da Adreno)
pkg install -y proot-distro x11-repo tur-repo
pkg update -y || true
pkg install -y termux-x11-nightly pulseaudio python
# GPU: virgl (desktop) + Turnip/Freedreno (Vulkan real da Adreno -> navegador via zink)
# mesa-vulkan-icd-freedreno-dri3 vem do tur-repo (igual phoenixbyrd/Termux_XFCE)
pkg install -y virglrenderer-android mesa-vulkan-icd-freedreno-dri3 || \
  pkg install -y virglrenderer-android || true
# ACELERACAO REAL: virgl com backend zink->Turnip (GPU da Adreno) acelera o
# desktop inteiro, inclusive o Firefox. Best-effort (nao aborta se faltar repo).
pkg install -y mesa-zink virglrenderer-mesa-zink vulkan-loader-android || true

echo ">> [2/4] instalando a base Debian (demora)..."
proot-distro install debian || echo "   (debian ja instalado)"

echo ">> [3/4] montando o desktop XFCE + apps do Olal dentro do Debian..."
proot-distro login debian -- bash -s < "$SCRIPTDIR/setup-desktop.sh"

echo ">> [4/4] instalando os atalhos..."
mkdir -p "$HOME/.olal"
rm -rf "$HOME/.olal/shell"; cp -r "$SCRIPTDIR/shell" "$HOME/.olal/shell"
cp "$SCRIPTDIR/olal"     "$HOME/olal";     chmod +x "$HOME/olal"
cp "$SCRIPTDIR/olal-web" "$HOME/olal-web"; chmod +x "$HOME/olal-web"
cp "$SCRIPTDIR/olal-update" "$HOME/olal-update"; chmod +x "$HOME/olal-update"
cp "$SCRIPTDIR/olal-doctor" "$HOME/olal-doctor"; chmod +x "$HOME/olal-doctor"

echo ""
echo "==================================================="
echo "   PRONTO!"
echo ""
echo "   ./olal      -> desktop Debian XFCE (tela cheia no"
echo "                  Termux:X11, com GPU). Abra o app"
echo "                  Termux:X11 depois de rodar."
echo ""
echo "   ./olal-web  -> abre so a interface do Olal no Chrome"
echo "                  do celular (sempre funciona)."
echo ""
echo "   Instale o app 'Termux:X11' (F-Droid) para o desktop."
echo "==================================================="
