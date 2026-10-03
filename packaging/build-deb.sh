#!/bin/sh
# Build lunduke-city_0.9-1_amd64.deb into packaging/debs/ (repo-local).
# Does NOT seed lcos-live-07 (Phil seeds by hand into packages.chroot).
# meson install ships Icon=lunduke-city into hicolor (SVG plus 16/32/48/64/128/256).
set -eu

ROOT="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
VERSION="0.9-1"
PKGNAME="lunduke-city_${VERSION}_amd64"
BUILD="$ROOT/build-deb"
DEST="$ROOT/packaging/src/lunduke-city"
DEB_DIR="$ROOT/packaging/debs"

cd "$ROOT"

rm -rf "$BUILD"
meson setup "$BUILD" --prefix=/usr --buildtype=release -Dstrip=true
meson compile -C "$BUILD"

rm -rf "$DEST"
meson install -C "$BUILD" --destdir "$DEST"

# Ensure GPL+EA NOTICE and COPYING ship in the package (meson install_data
# should place them; reinforce copy if missing).
mkdir -p "$DEST/usr/share/doc/lunduke-city"
cp -a "$ROOT/NOTICE" "$DEST/usr/share/doc/lunduke-city/NOTICE"
cp -a "$ROOT/COPYING" "$DEST/usr/share/doc/lunduke-city/COPYING"

mkdir -p "$DEST/debian"
cp "$ROOT/debian/control" "$DEST/debian/control"

SHLIBS="$(
  cd "$DEST"
  dpkg-shlibdeps --ignore-missing-info -O \
    -e usr/bin/lunduke-city
)"
SHLIBS_DEPS="${SHLIBS#shlibs:Depends=}"

SIZE="$(du -sk "$DEST/usr" | awk '{print $1}')"

mkdir -p "$DEST/DEBIAN"
cat > "$DEST/DEBIAN/control" << CTRL
Package: lunduke-city
Version: ${VERSION}
Section: games
Priority: optional
Architecture: amd64
Installed-Size: ${SIZE}
Maintainer: LCOS <lcos@lunduke.com>
Homepage: https://lunduke.com
Depends: ${SHLIBS_DEPS}, desktop-file-utils, gtk-update-icon-cache
Recommends: libpulse0
Description: Lunduke City, a gtkmm city-builder for LCOS
 Windowed city-building game for the Lunduke Computer Operating System.
 Classic menu / funds / tool-palette / map layout wired to the Micropolis
 simulation engine (tiles, sprites, budget, overlays, PulseAudio sounds).
 GPL-3.0-or-later with Electronic Arts additional terms; see NOTICE.
CTRL

cat > "$DEST/DEBIAN/postinst" << 'POST'
#!/bin/sh
set -e
if [ "$1" = "configure" ]; then
  if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || true
  fi
  if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q /usr/share/icons/hicolor >/dev/null 2>&1 || true
  fi
fi
exit 0
POST
chmod 0755 "$DEST/DEBIAN/postinst"

(
  cd "$DEST"
  find usr -type f -print0 | sort -z | xargs -0 md5sum > DEBIAN/md5sums
)

rm -rf "$DEST/debian"

mkdir -p "$DEB_DIR"
fakeroot dpkg-deb --root-owner-group --build "$DEST" "$DEB_DIR/${PKGNAME}.deb"

echo "built $DEB_DIR/${PKGNAME}.deb"
