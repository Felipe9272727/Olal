#!/bin/bash
# ============================================================
#  Olal OS - setup do desktop (roda DENTRO do Debian via proot).
#  Instala o XFCE, o Firefox e a identidade/apps do Olal.
# ============================================================
set -e
export DEBIAN_FRONTEND=noninteractive

echo ">> atualizando o Debian..."
apt-get update -y; apt-get upgrade -y || true

echo ">> instalando o desktop XFCE + Firefox + ferramentas..."
apt-get install -y --no-install-recommends \
    xfce4 xfce4-terminal xfce4-goodies xfdesktop4 \
    firefox-esr \
    dbus-x11 x11-xserver-utils mesa-utils \
    pulseaudio-utils \
    python3 git build-essential nano wget curl \
    fonts-dejavu fonts-noto-core fonts-noto-color-emoji || true

# ---------- identidade e apps do Olal ----------
mkdir -p /opt/olal
cd /root
[ -d olal-src ] || git clone --depth 1 https://github.com/Felipe9272727/Olal olal-src || true

# interface (shell) do Olal
[ -d olal-src/olal-os/shell ] && cp -r olal-src/olal-os/shell /opt/olal/shell

# OLA-32: nossa CPU (comando 'ola32' no terminal)
[ -f olal-src/arch/oemu.c ] && gcc -O2 olal-src/arch/oemu.c -o /usr/local/bin/ola32 2>/dev/null || true

# wallpaper + lancadores + wrapper
cp olal-src/olal-os/desktop/wallpaper.png /opt/olal/wallpaper.png 2>/dev/null || true
cp olal-src/olal-os/desktop/*.desktop /usr/share/applications/ 2>/dev/null || true
install -m755 olal-src/olal-os/desktop/olal-shell /usr/local/bin/olal-shell 2>/dev/null || true

# o navegador que renderiza (definido pelo launcher via $OLAL_BROWSER); padrao firefox
echo 'export OLAL_BROWSER=${OLAL_BROWSER:-firefox-esr}' > /etc/profile.d/olal.sh
cat >> /etc/profile.d/olal.sh <<'SH'
if [ -t 1 ]; then echo "  Olal OS (Debian) - comandos: ola32 | olal-shell | firefox-esr"; fi
SH

# wallpaper padrao do XFCE (aplicado quando o desktop sobe)
mkdir -p /root/.config/xfce4
echo "Olal OS (Debian XFCE)" > /etc/olal-release
echo ">> setup do desktop concluido."
