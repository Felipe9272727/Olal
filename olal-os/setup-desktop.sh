#!/bin/bash
# ============================================================
#  Olal OS - setup do desktop (roda DENTRO do Debian via proot).
#  Instala o XFCE, o Firefox e a identidade/apps do Olal.
# ============================================================
set -e
export DEBIAN_FRONTEND=noninteractive

echo ">> atualizando o Debian..."
apt-get update -y; apt-get upgrade -y || true

echo ">> instalando o desktop XFCE + Chromium + GPU (Turnip/zink)..."
apt-get install -y --no-install-recommends \
    xfce4 xfce4-terminal xfce4-goodies xfdesktop4 \
    chromium \
    dbus-x11 x11-xserver-utils \
    mesa-utils mesa-vulkan-drivers libvulkan1 vulkan-tools libgl1-mesa-dri \
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

# wallpaper + icone + lancadores + wrapper
cp olal-src/olal-os/desktop/wallpaper.png /opt/olal/wallpaper.png 2>/dev/null || true
cp olal-src/olal-os/desktop/icon.png /opt/olal/icon.png 2>/dev/null || true
cp olal-src/olal-os/desktop/*.desktop /usr/share/applications/ 2>/dev/null || true
install -m755 olal-src/olal-os/desktop/olal-shell /usr/local/bin/olal-shell 2>/dev/null || true

cat > /etc/profile.d/olal.sh <<'SH'
if [ -t 1 ]; then echo "  Olal OS (Debian) - comandos: ola32 | olal-shell | gpu-test"; fi
SH

# lancador "Testar GPU" (confirma virgl no desktop e Turnip/zink no navegador)
cat > /usr/local/bin/gpu-test <<'SH'
#!/bin/bash
echo "== Vulkan (tem que listar 'Turnip Adreno') =="
vulkaninfo 2>/dev/null | grep -i "deviceName" | head -2
echo ""
echo "== Desktop / virgl (deve dizer 'virgl', nao 'llvmpipe') =="
GALLIUM_DRIVER=virpipe glxinfo 2>/dev/null | grep -i "OpenGL renderer"
echo ""
echo "== Navegador / zink (deve dizer 'zink (Turnip Adreno...)') =="
MESA_LOADER_DRIVER_OVERRIDE=zink TU_DEBUG=noconform glxinfo 2>/dev/null | grep -i "OpenGL renderer"
SH
chmod +x /usr/local/bin/gpu-test
cat > /usr/share/applications/gpu-test.desktop <<'EOF'
[Desktop Entry]
Version=1.0
Type=Application
Name=Testar GPU (Olal)
Exec=xfce4-terminal --title="GPU" -e "bash -lc 'gpu-test; echo; read -p enter'"
Icon=utilities-system-monitor
Terminal=false
Categories=System;
EOF

# wallpaper do Olal: autostart que aplica no XFCE ao iniciar
mkdir -p /root/.config/autostart
cat > /root/.config/autostart/olal-wallpaper.desktop <<'EOF'
[Desktop Entry]
Type=Application
Name=Olal wallpaper
Exec=bash -c 'sleep 3; for p in $(xfconf-query -c xfce4-desktop -l 2>/dev/null | grep last-image); do xfconf-query -c xfce4-desktop -p "$p" -s /opt/olal/wallpaper.png; done; xfdesktop --reload'
X-GNOME-Autostart-enabled=true
EOF

echo "Olal OS (Debian XFCE)" > /etc/olal-release
echo ">> setup do desktop concluido."
