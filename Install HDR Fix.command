#!/bin/bash
#
# Witcher 3 HDR fix for Mac - installer
#
# Copies dinput8.dll next to witcher3.exe (bin/x64_dx12) and tells Wine to load it
# for witcher3.exe only. Works for Steam and GOG, in CrossOver and Whisky.
# Usage: double-click, or run: ./"Install HDR Fix.command" ["/path/to/The Witcher 3"]

HERE="$(cd "$(dirname "$0")" && pwd)"
DLL="$HERE/dinput8.dll"
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
say "  The Witcher 3 - HDR fix for Mac (install)"
say "=============================================="
say ""

if [ ! -f "$DLL" ]; then
    say "dinput8.dll is missing. Keep it in the same folder as this installer."
    finish 1
fi

while pgrep -if 'witcher3\.exe' >/dev/null 2>&1; do
    say "The Witcher 3 is running. Quit the game, then press Enter."
    read -r
done

# Turns a folder, its bin folder, bin/x64_dx12 or witcher3.exe into the bin/x64_dx12 folder.
resolve_game_dir() {
    local c="${1%/}"
    [ -f "$c" ] && c="$(dirname "$c")"
    for d in "$c" "$c/x64_dx12" "$c/bin/x64_dx12"; do
        if [ -f "$d/witcher3.exe" ] && [ "$(basename "$d")" = "x64_dx12" ]; then
            (cd -P "$d" && pwd -P)
            return
        fi
    done
}

games=()
add_game() {
    local r g
    r="$(resolve_game_dir "$1")"
    [ -n "$r" ] || return 0
    for g in "${games[@]}"; do [ "$g" = "$r" ] && return 0; done
    games+=("$r")
}

if [ -n "$1" ]; then
    add_game "$1"
else
    say "Searching for The Witcher 3..."
    shopt -s nullglob
    for root in "$HOME/Library/Application Support/CrossOver/Bottles"/*/drive_c \
                "$HOME/Library/Containers/com.isaacmarovitz.Whisky/Bottles"/*/drive_c; do
        while IFS= read -r exe; do add_game "$exe"; done \
            < <(find "$root" -path "*/bin/x64_dx12/witcher3.exe" 2>/dev/null)
    done
    shopt -u nullglob
fi

if [ ${#games[@]} -eq 0 ]; then
    say "Couldn't find the game automatically."
    say "Drag your Witcher 3 folder (the one containing 'bin' and 'content')"
    say "into this window and press Enter:"
    read -r manual
    manual="${manual%\'}"; manual="${manual#\'}"; manual="${manual//\\ / }"
    manual="${manual%"${manual##*[![:space:]]}"}"
    add_game "$manual"
    if [ ${#games[@]} -eq 0 ]; then
        say "No bin/x64_dx12/witcher3.exe found there."
        finish 1
    fi
fi

# The Wine prefix (bottle) is the folder that contains drive_c.
prefix_of() {
    case "$1" in
        */drive_c/*) printf '%s\n' "${1%%/drive_c/*}" ;;
    esac
}

set_override() {
    local prefix="$1"
    case "$prefix" in
        "$HOME/Library/Application Support/CrossOver/Bottles/"*)
            [ -x "$CX_WINE" ] || return 1
            "$CX_WINE" --bottle "$(basename "$prefix")" reg add "$REG_KEY" /v dinput8 /d native,builtin /f >/dev/null 2>&1
            ;;
        *)
            [ -x "$WHISKY_WINE" ] || return 1
            WINEPREFIX="$prefix" "$WHISKY_WINE" reg add "$REG_KEY" /v dinput8 /d native,builtin /f >/dev/null 2>&1
            ;;
    esac
}

failed=0
for g in "${games[@]}"; do
    say ""
    say "Found: $g"
    target="$g/dinput8.dll"
    if [ -f "$target" ] && ! grep -q "$MARKER" "$target" && [ ! -e "$target.before-hdrfix" ]; then
        mv "$target" "$target.before-hdrfix"
        say "   - existing dinput8.dll (another mod?) saved as dinput8.dll.before-hdrfix"
    fi
    if cp "$DLL" "$target"; then
        say "   + dinput8.dll installed"
    else
        say "   ! could not copy dinput8.dll"
        failed=1
        continue
    fi
    prefix="$(prefix_of "$g")"
    if [ -n "$prefix" ] && set_override "$prefix"; then
        say "   + Wine set to load it for witcher3.exe"
    else
        say "   ! Could not set the Wine DLL override automatically. Do it by hand:"
        say "     Wine configuration > Libraries > add 'dinput8' > set it to 'Native then Builtin'."
        failed=1
    fi
done

say ""
if [ $failed -eq 0 ]; then
    say "Done. Start the game as usual; it will run in SDR (no washed-out HDR)."
    say "Check: the log witcher3-hdrfix.log in the bottle's users folder should end with"
    say "       'HDR is OFF'."
else
    say "Finished with problems, see the messages above."
fi
finish $failed
