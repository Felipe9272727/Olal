#!/data/data/com.termux/files/usr/bin/bash
# ============================================================
#  Olal OS - inicia a interface no seu celular.
#  Sobe o audio, a tela (Termux:X11) e o Olal em tela cheia.
# ============================================================

# 1) audio (modulo TCP carregado junto com o daemon -> YouTube com som)
pulseaudio --start --exit-idle-time=-1 \
  --load="module-native-protocol-tcp auth-ip-acl=127.0.0.1 auth-anonymous=1" 2>/dev/null || true

# 2) servidor grafico (Termux:X11). O socket vai pro $TMPDIR para casar com o
#    --shared-tmp do proot (e assim o Chromium dentro do Debian enxerga o X11).
pkill -f termux-x11 2>/dev/null || true
export TMPDIR="$PREFIX/tmp"; mkdir -p "$TMPDIR"
termux-x11 :0 -ac &
sleep 2
# abre o app Termux:X11 automaticamente (a renderizacao acontece nele)
am start --user 0 -n com.termux.x11/com.termux.x11.MainActivity >/dev/null 2>&1 || true

echo ""
echo ">> Abrindo o Olal na tela do Termux:X11..."
echo "   (YouTube e Navegador rodam no Chromium real do celular)"
read -t 6 -p "   tecle ENTER para iniciar a interface (ou aguarde)... " _ || true

# 3) sobe a interface do Olal dentro do Debian
proot-distro login debian --shared-tmp -- bash -lc "olal-shell"
