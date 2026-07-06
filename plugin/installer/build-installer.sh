#!/usr/bin/env bash
# Builds ASTRA-TUNE-<version>.pkg — a macOS installer with selectable
# AU / VST3 / Standalone / AAX components. AAX is included automatically
# when it exists in the build artefacts (i.e. after building with
# -DAAX_SDK_PATH). Run from anywhere; requires a completed plugin build.
set -euo pipefail

cd "$(dirname "$0")/.."
VERSION="1.0.0"
IDENT="com.companyaistack.astratune"
ART="build/AstraTune_artefacts/Release"
STAGE="build/installer-stage"
PKGS="$STAGE/pkgs"
RES="installer/resources"
OUT="build/ASTRA-TUNE-$VERSION.pkg"

if [ ! -d "$ART" ]; then
  echo "error: no build artefacts at $ART — build the plugin first:" >&2
  echo "  cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j8" >&2
  exit 1
fi

rm -rf "$STAGE"
mkdir -p "$PKGS"

CHOICE_LINES=""
CHOICE_DEFS=""

add_component() { # id  title  description  bundle-path  install-location
  local id="$1" title="$2" desc="$3" src="$4" dest="$5"
  if [ ! -e "$src" ]; then
    echo "  skipping $title (not built: $src)"
    return 0
  fi
  local root="$STAGE/root-$id"
  mkdir -p "$root"
  cp -R "$src" "$root/"
  pkgbuild --root "$root" \
           --install-location "$dest" \
           --identifier "$IDENT.$id" \
           --version "$VERSION" \
           "$PKGS/$id.pkg" > /dev/null
  echo "  packaged $title -> $dest"
  CHOICE_LINES+="    <line choice=\"$id\"/>
"
  CHOICE_DEFS+="  <choice id=\"$id\" title=\"$title\" description=\"$desc\">
    <pkg-ref id=\"$IDENT.$id\"/>
  </choice>
  <pkg-ref id=\"$IDENT.$id\" version=\"$VERSION\">$id.pkg</pkg-ref>
"
}

echo "Staging components..."
add_component aax "AAX (Pro Tools)" \
  "The AAX plugin, for Avid Pro Tools." \
  "$ART/AAX/ASTRA-TUNE.aaxplugin" \
  "/Library/Application Support/Avid/Audio/Plug-Ins"
add_component au "Audio Unit (AU)" \
  "The AU plugin, for Logic Pro, GarageBand, and other AU hosts." \
  "$ART/AU/ASTRA-TUNE.component" \
  "/Library/Audio/Plug-Ins/Components"
add_component vst3 "VST3" \
  "The VST3 plugin, for Ableton Live, Cubase, Reaper, and other VST3 hosts." \
  "$ART/VST3/ASTRA-TUNE.vst3" \
  "/Library/Audio/Plug-Ins/VST3"
add_component app "Standalone Application" \
  "ASTRA-TUNE as a standalone app (mic in, corrected audio out)." \
  "$ART/Standalone/ASTRA-TUNE.app" \
  "/Applications"

if [ -z "$CHOICE_LINES" ]; then
  echo "error: nothing to package" >&2
  exit 1
fi

if [ ! -d "$ART/AAX" ]; then
  echo "  note: AAX not present — rebuild with -DAAX_SDK_PATH=<sdk> to include Pro Tools support"
fi

cat > "$STAGE/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>ASTRA-TUNE $VERSION</title>
  <welcome file="welcome.html"/>
  <license file="license.txt"/>
  <conclusion file="conclusion.html"/>
  <options customize="always" require-scripts="false" rootVolumeOnly="true"/>
  <domains enable_localSystem="true"/>
  <choices-outline>
$CHOICE_LINES  </choices-outline>
$CHOICE_DEFS</installer-gui-script>
EOF

productbuild --distribution "$STAGE/distribution.xml" \
             --package-path "$PKGS" \
             --resources "$RES" \
             "$OUT" > /dev/null

echo
echo "Installer written to: $OUT"
du -h "$OUT" | cut -f1 | xargs echo "Size:"
echo
echo "Note: the package is unsigned. Local installs work (right-click > Open"
echo "or: sudo installer -pkg \"$OUT\" -target /). For public distribution,"
echo "sign with a Developer ID Installer certificate and notarize:"
echo "  productsign --sign \"Developer ID Installer: <name>\" \"$OUT\" signed.pkg"
echo "  xcrun notarytool submit signed.pkg --keychain-profile <profile> --wait"
