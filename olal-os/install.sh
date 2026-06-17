#!/data/data/com.termux/files/usr/bin/bash
# ============================================================
#  Olal OS - instalador para Android (Termux, SEM root).
#  Usa o Chromium NATIVO do Termux (tur-repo), que renderiza no
#  Termux:X11 - resolve a tela preta do Chromium do proot no arm64.
#  NAO apaga o Android. NAO precisa de root.
# ============================================================
set -e
SCRIPTDIR="$(cd "$(dirname "$0")" && pwd)"

echo "==================================================="
echo "   Olal OS  -  instalando no seu celular"
echo "==================================================="

echo ">> [1/3] atualizando o Termux e habilitando repositorios..."
yes | pkg update || true
pkg install -y x11-repo tur-repo
pkg update -y || true

echo ">> [2/3] instalando Chromium nativo + tela + audio (pode demorar)..."
pkg install -y termux-x11-nightly chromium python pulseaudio || {
  echo "   AVISO: algum pacote falhou. Tente de novo com boa conexao."; }

echo ">> [3/3] instalando a interface do Olal..."
mkdir -p "$HOME/.olal"
rm -rf "$HOME/.olal/shell"
cp -r "$SCRIPTDIR/shell" "$HOME/.olal/shell"
cp "$SCRIPTDIR/olal"     "$HOME/olal";     chmod +x "$HOME/olal"
cp "$SCRIPTDIR/olal-web" "$HOME/olal-web"; chmod +x "$HOME/olal-web"

echo ""
echo "==================================================="
echo "   PRONTO! Duas formas de abrir o Olal:"
echo ""
echo "   ./olal-web   -> abre no navegador do celular"
echo "                   (RECOMENDADO: renderiza 100%, com"
echo "                    YouTube, IA e touch)"
echo ""
echo "   ./olal       -> tela cheia no Termux:X11 (Chromium"
echo "                   nativo). Precisa do app Termux:X11."
echo ""
echo "   Instale tambem o app 'Termux:X11' pelo F-Droid se for"
echo "   usar o modo tela cheia."
echo "==================================================="
