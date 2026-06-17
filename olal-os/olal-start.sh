#!/data/data/com.termux/files/usr/bin/bash
# ============================================================
#  Olal OS - inicia a interface no seu celular.
#  Sobe o audio, a tela (Termux:X11) e o Olal em tela cheia.
# ============================================================

# 1) audio (pra YouTube ter som)
pulseaudio --start --exit-idle-time=-1 2>/dev/null || true
pacmd load-module module-native-protocol-tcp auth-ip-acl=127.0.0.1 auth-anonymous=1 2>/dev/null || true

# 2) servidor grafico (Termux:X11)
pkill -f "termux-x11 :0" 2>/dev/null || true
export XDG_RUNTIME_DIR="$PREFIX/tmp"
termux-x11 :0 -ac >/dev/null 2>&1 &
sleep 3

echo ""
echo ">> Abra agora o app 'Termux:X11' para ver o Olal OS."
echo "   (YouTube e Navegador rodam no Chromium real do celular)"
echo ""

# 3) sobe a interface do Olal dentro do Debian
proot-distro login debian --shared-tmp -- bash -lc "olal-shell"
