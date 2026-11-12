#!/bin/bash
# Installe Aatini Drums (VST3 + AU) pour l'utilisateur courant.
# Usage : ouvre le Terminal, tape  bash  puis glisse ce fichier dans la fenetre, puis Entree.
cd "$(dirname "$0")" || exit 1

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

# Retire le blocage "telecharge depuis internet" de macOS
xattr -cr "Aatini Drums.vst3" "Aatini Drums.component" 2>/dev/null

rm -rf "$VST3_DIR/Aatini Drums.vst3" "$AU_DIR/Aatini Drums.component"
cp -R "Aatini Drums.vst3" "$VST3_DIR/"
cp -R "Aatini Drums.component" "$AU_DIR/"

# Force macOS a relire les Audio Units
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo "Installe. Relance Ableton puis scanne les plug-ins (Preferences > Plug-ins, Option + Rescanner)."
