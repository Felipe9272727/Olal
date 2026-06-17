#!/data/data/com.termux/files/usr/bin/bash
# ============================================================
#  Olal layer - adiciona a cara e os apps do Olal a um desktop
#  Debian XFCE JA instalado (ex.: phoenixbyrd/Termux_XFCE, que
#  ja deixa a GPU Turnip/zink funcionando no Adreno).
#  Rode DEPOIS de ter o desktop funcionando.
# ============================================================
set -e
DEB="$PREFIX/var/lib/proot-distro/installed-rootfs/debian"
[ -d "$DEB" ] || { echo "Debian proot nao encontrado em $DEB"; exit 1; }

echo ">> baixando o Olal..."
rm -rf /tmp/olal-src
git clone --depth 1 https://github.com/Felipe9272727/Olal /tmp/olal-src

echo ">> injetando o Olal no Debian..."
mkdir -p "$DEB/opt/olal"
cp -r /tmp/olal-src/olal-os/shell      "$DEB/opt/olal/shell"
cp /tmp/olal-src/olal-os/desktop/wallpaper.png "$DEB/opt/olal/wallpaper.png"
cp /tmp/olal-src/olal-os/desktop/icon.png      "$DEB/opt/olal/icon.png"
cp /tmp/olal-src/arch/oemu.c           "$DEB/opt/olal/oemu.c"
install -m755 /tmp/olal-src/olal-os/desktop/olal-shell "$DEB/usr/local/bin/olal-shell"
cp /tmp/olal-src/olal-os/desktop/olal-ai.desktop "$DEB/usr/share/applications/" 2>/dev/null || true
cp /tmp/olal-src/olal-os/desktop/ola32.desktop   "$DEB/usr/share/applications/" 2>/dev/null || true

echo ">> compilando OLA-32 e configurando dentro do Debian..."
proot-distro login debian --shared-tmp -- /bin/bash -c '
  apt-get update -y >/dev/null 2>&1 || true
  # firefox-esr e o navegador padrao (render comprovado por software/GTK);
  # chromium fica como alternativa acelerada por zink/Turnip.
  apt-get install -y --no-install-recommends python3 firefox-esr chromium build-essential >/dev/null 2>&1 || true
  gcc -O2 /opt/olal/oemu.c -o /usr/local/bin/ola32 2>/dev/null || true
  # wallpaper do Olal
  mkdir -p ~/.config/autostart
  cat > ~/.config/autostart/olal-wallpaper.desktop <<EOF
[Desktop Entry]
Type=Application
Name=Olal wallpaper
Exec=bash -c "sleep 3; for p in \$(xfconf-query -c xfce4-desktop -l 2>/dev/null | grep last-image); do xfconf-query -c xfce4-desktop -p \"\$p\" -s /opt/olal/wallpaper.png; done; xfdesktop --reload"
EOF
'
echo ""
echo "==================================================="
echo "  Olal adicionado! No desktop, abra o app 'Olal AI'"
echo "  (ou rode 'olal-shell' no terminal do Debian)."
echo "==================================================="
