#!/bin/bash
# ============================================================
#  Olal OS - setup do desktop (roda DENTRO do Debian via proot).
#  Instala o XFCE, o Firefox e a identidade/apps do Olal.
# ============================================================
set -e
export DEBIAN_FRONTEND=noninteractive

echo ">> atualizando o Debian..."
apt-get update -y; apt-get upgrade -y || true

echo ">> instalando o Olal OS: WM minimo (openbox) + navegador + GPU..."
# Olal E a interface (nao usamos um desktop XFCE). Openbox so gerencia janelas.
apt-get install -y --no-install-recommends \
    openbox \
    firefox-esr chromium \
    dbus-x11 x11-xserver-utils \
    mesa-utils mesa-vulkan-drivers libvulkan1 vulkan-tools libgl1-mesa-dri \
    pulseaudio-utils imagemagick \
    python3 git build-essential nano wget curl \
    fonts-dejavu fonts-noto-core fonts-noto-color-emoji || true
# o Firefox renderiza pelo mesmo caminho GTK/X11 do desktop (mais confiavel
# que o Chromium); o Chromium fica como alternativa acelerada por zink.

# ---------- tuning do sistema: apt/dpkg mais leve e rapido + cache de fontes ----------
printf 'APT::Install-Recommends "false";\nAPT::Install-Suggests "false";\nAcquire::Languages "none";\n' > /etc/apt/apt.conf.d/99olal
printf 'path-exclude /usr/share/doc/*\npath-exclude /usr/share/man/*\npath-exclude /usr/share/info/*\npath-exclude /usr/share/locale/*/LC_MESSAGES/*.mo\n' > /etc/dpkg/dpkg.cfg.d/99olal-nodoc
# pre-gera o cache de fontes (1o boot do navegador nao trava montando fontes)
fc-cache -f >/dev/null 2>&1 || true

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

# diagnostico completo do Olal: checa cada componente (PASS/FAIL)
cat > /usr/local/bin/olal-doctor <<'SH'
#!/bin/bash
ok(){ echo -e "  [\e[32mOK\e[0m]  $1"; }
no(){ echo -e "  [\e[31mFALHA\e[0m] $1"; }
echo "===== Olal doctor ====="
[ -n "$DISPLAY" ] && xdpyinfo >/dev/null 2>&1 && ok "tela X11 ($DISPLAY)" || no "tela X11 - rode dentro do desktop"
vulkaninfo 2>/dev/null | grep -iq adreno && ok "GPU Vulkan: Turnip/Adreno" || no "Vulkan Turnip (vai usar software)"
GALLIUM_DRIVER=virpipe glxinfo 2>/dev/null | grep -i "OpenGL renderer" | grep -iq virgl && ok "GL desktop: virgl" || no "GL desktop (virgl)"
MESA_LOADER_DRIVER_OVERRIDE=zink glxinfo 2>/dev/null | grep -i "OpenGL renderer" | grep -iq zink && ok "GL navegador: zink/Turnip" || no "GL navegador (zink)"
command -v firefox-esr >/dev/null && ok "Firefox instalado" || no "Firefox"
command -v chromium >/dev/null && ok "Chromium instalado" || no "Chromium"
command -v python3 >/dev/null && ok "python3" || no "python3"
[ -f /opt/olal/shell/index.html ] && ok "interface do Olal presente" || no "shell do Olal (/opt/olal/shell)"
command -v ola32 >/dev/null && ok "OLA-32 (nossa CPU)" || no "OLA-32"
echo "======================="
echo "Para abrir a interface:  olal-shell   (ou  olal-shell chromium)"
SH
chmod +x /usr/local/bin/olal-doctor

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

# ---------- IDENTIDADE DE OS: o sistema se chama Olal OS, nao Debian ----------
echo "Olal OS 1.0" > /etc/olal-release
# os-release (o que neofetch/fastfetch e o sistema leem)
cat > /etc/os-release <<'EOF'
PRETTY_NAME="Olal OS 1.0 (movido por IA)"
NAME="Olal OS"
VERSION_ID="1.0"
VERSION="1.0"
ID=olal
ID_LIKE=debian
HOME_URL="https://felipe9272727.github.io/Olal/"
EOF
echo "olal" > /etc/hostname
echo "Bem-vindo ao Olal OS - Linux real, movido por IA." > /etc/motd
cat > /etc/issue <<'EOF'
Olal OS 1.0 \n \l
EOF

# banner do Olal ao abrir o terminal (logo ASCII + prompt proprio)
cat > /etc/profile.d/olal.sh <<'SH'
if [ -t 1 ]; then
cat <<'BANNER'
   ___  _       _    ___  ___
  / _ \| | __ _| |  / _ \/ __|   Olal OS 1.0
 | (_) | |/ _` | | | (_) \__ \   Linux real, movido por IA
  \___/|_|\__,_|_|  \___/|___/   apps: ola32 | olal-shell | gpu-test
BANNER
fi
export PS1='\[\e[38;5;42m\]olal\[\e[0m\]:\[\e[38;5;39m\]\w\[\e[0m\]\$ '
SH

echo ">> setup do desktop (Olal OS) concluido."
