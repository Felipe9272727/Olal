#!/bin/bash
# ============================================================
#  Olal OS - setup interno (roda DENTRO do Debian via proot).
#  Instala o Chromium (motor real -> YouTube), a interface do
#  Olal (shell) em tela cheia, e porta os apps do Olal.
# ============================================================
set -e
export DEBIAN_FRONTEND=noninteractive

echo ">> atualizando o Debian..."
apt-get update -y
apt-get upgrade -y || true

echo ">> instalando Chromium + base grafica + ferramentas..."
apt-get install -y --no-install-recommends \
    chromium \
    openbox \
    dbus-x11 x11-xserver-utils \
    pulseaudio-utils \
    fonts-dejavu fonts-noto-core fonts-noto-color-emoji \
    python3 git build-essential nano wget curl ca-certificates || \
  apt-get install -y --no-install-recommends chromium-browser openbox dbus-x11 \
    x11-xserver-utils pulseaudio-utils fonts-dejavu python3 git nano wget curl ca-certificates || true

# ---------- interface do Olal (nossa cara + todos os apps) ----------
mkdir -p /opt/olal
cd /root
if [ ! -d olal-src ]; then
    git clone --depth 1 https://github.com/Felipe9272727/Olal olal-src || true
fi
if [ -d olal-src/olal-os/shell ]; then
    cp -r olal-src/olal-os/shell /opt/olal/shell
else
    echo "<h1>Olal shell nao encontrado</h1>" > /opt/olal/shell.html
fi

# OLA-32 nativo tambem disponivel no terminal (alem do app em JS)
if [ -f olal-src/arch/oemu.c ]; then
    gcc -O2 olal-src/arch/oemu.c -o /usr/local/bin/ola32 2>/dev/null || true
fi

# ---------- comando que sobe a interface do Olal em tela cheia ----------
CHROME=chromium
command -v chromium >/dev/null 2>&1 || CHROME=chromium-browser
cat > /usr/local/bin/olal-shell <<EOF
#!/bin/bash
export DISPLAY=:0
export PULSE_SERVER=tcp:127.0.0.1
export XDG_RUNTIME_DIR=/run/olal
mkdir -p \$XDG_RUNTIME_DIR; chmod 700 \$XDG_RUNTIME_DIR
# serve a interface por HTTP local (o YouTube embed exige origem http, nao file://)
( cd /opt/olal/shell && python3 -m http.server 8080 >/dev/null 2>&1 & )
sleep 1
# gerenciador de janelas minimo (foco/teclado pro Chromium)
openbox &
sleep 1
# Olal em tela cheia (quiosque). Motor real -> YouTube toca de verdade.
# loop: se o Chromium fechar/crashar (acontece em arm64/proot), reinicia.
while true; do
  $CHROME --kiosk --no-sandbox --disable-setuid-sandbox --no-zygote \\
    --disable-dev-shm-usage --disable-gpu --use-gl=swiftshader \\
    --autoplay-policy=no-user-gesture-required \\
    --start-fullscreen --force-device-scale-factor=1 \\
    --user-data-dir=/root/.olal-chrome \\
    http://localhost:8080/index.html
  echo "Olal: a interface fechou. Reiniciando em 2s... (Ctrl+C para sair)"
  sleep 2
done
EOF
chmod +x /usr/local/bin/olal-shell

echo "Olal OS (Linux) - interface propria sobre Debian" > /etc/olal-release
echo ">> setup do Olal concluido."
