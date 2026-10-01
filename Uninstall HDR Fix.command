#!/bin/bash
#
# Witcher 3 HDR fix for Mac - uninstaller
#
# Removes dinput8.dll from the game folder (only if it is this fix), restores a
# dinput8.dll that was there before, and removes the Wine DLL override.

MARKER="Witcher 3 HDR fix"
CX_WINE="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine"
WHISKY_WINE="$HOME/Library/Application Support/com.isaacmarovitz.Whisky/Libraries/Wine/bin/wine64"
REG_KEY='HKCU\Software\Wine\AppDefaults\witcher3.exe\DllOverrides'

say() { printf '%s\n' "$*"; }

finish() {
    say ""
    read -n 1 -s -r -p "Press any key to close this window..."
    say ""
    exit "$1"
}

say "=============================================="
say "  The Witcher 3 - HDR fix for Mac (uninstall)"
say "=============================================="
say ""

while pgrep -if 'witcher3\.exe' >/dev/null 2>&1; do
    say "The Witcher 3 is running. Quit the game, then press Enter."
    read -r
done

found=0
shopt -s nullglob
for root in "$HOME/Library/Application Support/CrossOver/Bottles"/*/drive_c \
            "$HOME/Library/Containers/com.isaacmarovitz.Whisky/Bottles"/*/drive_c; do
    while IFS= read -r dll; do
        grep -q "$MARKER" "$dll" || continue
        dir="$(dirname "$dll")"
        found=1
        say "Found: $dir"
        rm -f "$dll"
        if [ -e "$dll.before-hdrfix" ]; then
            mv "$dll.before-hdrfix" "$dll"
            say "   + previous dinput8.dll restored"
        fi
        say "   + HDR fix removed"
        # Only drop the override if no other dinput8.dll is left to need it.
        if [ ! -e "$dll" ]; then
            prefix="${root%/drive_c}"
            case "$prefix" in
                "$HOME/Library/Application Support/CrossOver/Bottles/"*)
                    [ -x "$CX_WINE" ] && "$CX_WINE" --bottle "$(basename "$prefix")" reg delete "$REG_KEY" /v dinput8 /f >/dev/null 2>&1 ;;
                *)
                    [ -x "$WHISKY_WINE" ] && WINEPREFIX="$prefix" "$WHISKY_WINE" reg delete "$REG_KEY" /v dinput8 /f >/dev/null 2>&1 ;;
            esac
            say "   + Wine override removed"
        fi
        say ""
    done < <(find "$root" -path "*/bin/x64_dx12/dinput8.dll" 2>/dev/null)
done
shopt -u nullglob

if [ $found -eq 0 ]; then
    say "The HDR fix is not installed."
else
    say "Done. The game is back to its normal HDR behaviour."
fi
finish 0
