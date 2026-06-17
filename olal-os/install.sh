#!/data/data/com.termux/files/usr/bin/bash
# ============================================================
#  Olal OS - instalador para Android (roda no Termux, SEM root)
#  Instala um Linux real (Debian) por cima do Android, com
#  desktop, Firefox de verdade e os apps do Olal.
#  NAO apaga o seu Android. NAO precisa de root.
# ============================================================
set -e
SCRIPTDIR="$(cd "$(dirname "$0")" && pwd)"

echo "==================================================="
echo "   Olal OS  -  Linux real no seu celular"
echo "   (sem root, sem apagar o Android)"
echo "==================================================="

echo ">> [1/4] atualizando o Termux..."
yes | pkg update || true
yes | pkg upgrade || true

echo ">> [2/4] instalando dependencias (proot, audio, tela)..."
pkg install -y proot-distro pulseaudio x11-repo
pkg update -y || true        # indexa o x11-repo recem-adicionado
if ! pkg install -y termux-x11-nightly; then
  echo "   AVISO: 'termux-x11-nightly' nao instalou. Instale o APP 'Termux:X11'"
  echo "   pelo F-Droid (a MESMA versao do pacote) e rode este install de novo."
fi

echo ">> [3/4] instalando a base Debian (pode demorar bastante)..."
proot-distro install debian || echo "   (debian ja estava instalado, seguindo)"

echo ">> [4/4] configurando o Olal dentro do Debian..."
proot-distro login debian -- bash -s < "$SCRIPTDIR/olal-setup.sh"

# launcher na home do Termux
cp "$SCRIPTDIR/olal-start.sh" "$HOME/olal"
chmod +x "$HOME/olal"

echo ""
echo "==================================================="
echo "   PRONTO!  Como ligar o Olal OS:"
echo ""
echo "   1) Instale o app 'Termux:X11' (pelo F-Droid)."
echo "   2) No Termux, rode:   ./olal"
echo "   3) Abra o app 'Termux:X11' para ver a tela."
echo ""
echo "   Dentro do Olal: o Firefox roda YouTube de verdade."
echo "==================================================="
