#!/bin/bash
# ============================================================
#  Olal OS - setup do desktop (roda DENTRO do Debian via proot).
#  Instala o WM (openbox) + navegador + GPU + a IDENTIDADE e os
#  apps do Olal. So isso: o desktop E o Olal.
# ============================================================
set -e
export DEBIAN_FRONTEND=noninteractive

echo ">> atualizando o Debian (apt configurado pra ser leve)..."
# tuning do apt/dpkg ANTES do install (ja vale pra essa passada)
printf 'APT::Install-Recommends "false";\nAPT::Install-Suggests "false";\nAcquire::Languages "none";\nAcquire::Retries "2";\nDPkg::Options {"--force-confold";};\n' > /etc/apt/apt.conf.d/99olal
printf 'path-exclude /usr/share/doc/*\npath-exclude /usr/share/man/*\npath-exclude /usr/share/info/*\npath-exclude /usr/share/locale/*/LC_MESSAGES/*.mo\n' > /etc/dpkg/dpkg.cfg.d/99olal-nodoc
apt-get update -y; apt-get upgrade -y || true

echo ">> instalando o Olal OS: WM minimo + app integration + navegador + GPU..."
# PACOTES:
#  - openbox: WM (so gerencia janelas, maximiza tudo, sem decoracao -> mobile-like)
#  - tint2: barra do topo (botao Olal + lista de janelas abertas)
#  - wmctrl + xdotool: integracao de apps (focus/lista/launch_integrated no backend)
#  - firefox-esr + chromium: navegadores (Firefox = software garantido,
#    Chromium = GPU acelerada por Turnip/zink)
#  - mesa/vulkan: drivers (virgl no desktop, Turnip na Adreno)
apt-get install -y --no-install-recommends \
    openbox tint2 wmctrl xdotool \
    firefox-esr chromium \
    dbus-x11 x11-xserver-utils \
    mesa-utils mesa-vulkan-drivers libvulkan1 vulkan-tools libgl1-mesa-dri \
    pulseaudio-utils imagemagick \
    python3 git build-essential nano wget curl ca-certificates \
    fonts-dejavu fonts-noto-core fonts-noto-color-emoji || true

# ---------- tuning do sistema (so o que REALMENTE funciona no proot) ----------
# OBS: no proot SEM root nao ha systemd nem PAM, entao `systemctl disable`,
# `sysctl` e `/etc/security/limits.conf` NAO tem efeito (sao ignorados). Por
# isso nao usamos esses (era cargo-cult). O que funciona de verdade:
# 1) cache de fontes (1o boot do navegador nao trava montando fontes)
fc-cache -f >/dev/null 2>&1 || true
# 2) desliga as tarefas periodicas do apt (essa config E lida pelo apt mesmo
#    sem systemd) -> nada de update/upgrade automatico comendo CPU/IO
printf 'APT::Periodic::Enable "0";\nAPT::Periodic::Update-Package-Lists "0";\nAPT::Periodic::Unattended-Upgrade "0";\n' > /etc/apt/apt.conf.d/99olal-noperiodic
# 3) ulimit de arquivos abertos: aplicado por shell (funciona no proot, ao
#    contrario do limits.conf) -> bom pro navegador
grep -q 'ulimit -n' /etc/profile.d/olal.sh 2>/dev/null || \
  echo 'ulimit -n 8192 2>/dev/null || true' >> /etc/profile.d/olal-ulimit.sh

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
cp olal-src/olal-os/desktop/icon.png      /opt/olal/icon.png      2>/dev/null || true
cp olal-src/olal-os/desktop/*.desktop /usr/share/applications/ 2>/dev/null || true
install -m755 olal-src/olal-os/desktop/olal-shell /usr/local/bin/olal-shell 2>/dev/null || true

# ---------- CONFIGURACOES DO WM E DA BARRA (integracao de apps) ----------
mkdir -p /etc/olal /root/.config/openbox /root/.config/tint2
[ -f olal-src/olal-os/desktop/olal-rc.xml ] && \
  cp olal-src/olal-os/desktop/olal-rc.xml /root/.config/openbox/rc.xml && \
  cp olal-src/olal-os/desktop/olal-rc.xml /etc/olal/openbox-rc.xml
[ -f olal-src/olal-os/desktop/olal-tint2rc ] && \
  cp olal-src/olal-os/desktop/olal-tint2rc /root/.config/tint2/tint2rc && \
  cp olal-src/olal-os/desktop/olal-tint2rc /etc/olal/tint2rc
# menu.xml vazio pro openbox (nao usamos menu de botao direito - mobile-like)
cat > /root/.config/openbox/menu.xml <<'X'
<openbox_menu><menu id="root-menu" label="Olal"></menu></openbox_menu>
X

# ---------- botao "Olal" da barra (volta pra interface do Olal) ----------
# Launcher do tint2 chama este .desktop -> chama olal-back -> traz navegador
# do Olal (Firefox/Chromium) de volta para frente, mesmo com apps abertos.
cat > /usr/local/bin/olal-back <<'SH'
#!/bin/bash
# traz o navegador do Olal de volta para frente. Tenta firefox -> chromium.
# (chamado pela barra do Olal no topo - botao "Olal")
for win in $(wmctrl -lx 2>/dev/null | awk '/[Ff]irefox|firefox-esr/ {print $1; exit}'); do
  wmctrl -ia "$win" 2>/dev/null && exit 0
done
for win in $(wmctrl -lx 2>/dev/null | awk '/[Cc]hromium/ {print $1; exit}'); do
  wmctrl -ia "$win" 2>/dev/null && exit 0
done
# fallback: se nao tem navegador do Olal aberto, abre o shell
olal-shell 2>/dev/null &
exit 0
SH
chmod +x /usr/local/bin/olal-back
cat > /usr/share/applications/olal-back.desktop <<'EOF'
[Desktop Entry]
Type=Application
Name=Olal
Exec=olal-back
Icon=go-home
Terminal=false
NoDisplay=true
StartupNotify=false
EOF

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
command -v wmctrl   >/dev/null && ok "wmctrl (integracao de apps)" || no "wmctrl (rode: apt install wmctrl)"
command -v xdotool  >/dev/null && ok "xdotool (integracao de apps)" || no "xdotool (rode: apt install xdotool)"
command -v tint2    >/dev/null && ok "tint2 (barra do Olal)" || no "tint2 (rode: apt install tint2)"
[ -f /root/.config/openbox/rc.xml ] && ok "openbox configurado (maximize-all)" || no "openbox sem config (apps abrem em janelas)"
[ -f /root/.config/tint2/tint2rc ] && ok "tint2 configurado (barra do Olal)" || no "tint2 sem config"
[ -f /usr/local/bin/olal-back ] && ok "botao Olal (volta pra interface)" || no "botao Olal (olal-back)"
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
