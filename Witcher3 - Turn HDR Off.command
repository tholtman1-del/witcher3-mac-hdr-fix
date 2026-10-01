#!/bin/bash
#
# Witcher 3 (Next-Gen / Remastered) - Turn HDR Off
#
# The game turns HDR on by itself on Macs with HDR screens, and when it runs
# through CrossOver / Whisky / Wine there is no HDR option in the menu, so the
# picture looks washed out and overblown. This script finds the game's config
# files (Steam or GOG, any bottle) and sets HdrEnabled=false.
#
# Double-click to run. Close the game first.

VALUE=false
CR=$'\r'
GAME_DIR="The Witcher 3"
CONFIG_FILES="dx12user.settings dx12user.settings.bak user.settings user.settings.bak"

say() { printf '%s\n' "$*"; }

finish() {
    say ""
    read -n 1 -s -r -p "Press any key to close this window..."
    say ""
    exit "$1"
}

say "=============================================="
say "  The Witcher 3 - Turn HDR Off"
say "=============================================="
say ""

# The game writes its settings when it quits, which would undo our change.
while pgrep -if 'witcher3\.exe' >/dev/null 2>&1; do
    say "The Witcher 3 is still running. Quit the game, then press Enter."
    read -r
done

# Every Windows "Documents" folder we know how to find.
shopt -s nullglob
candidates=(
    "$HOME/Documents/$GAME_DIR"
    # CrossOver
    "$HOME/Library/Application Support/CrossOver/Bottles"/*/drive_c/users/*/Documents/"$GAME_DIR"
    "$HOME/Library/Application Support/CrossOver/Bottles"/*/drive_c/users/*/"My Documents"/"$GAME_DIR"
    # Whisky
    "$HOME/Library/Containers/com.isaacmarovitz.Whisky/Bottles"/*/drive_c/users/*/Documents/"$GAME_DIR"
    # Heroic
    "$HOME/Games/Heroic/Prefixes"/*/*/drive_c/users/*/Documents/"$GAME_DIR"
    "$HOME/Games/Heroic/Prefixes"/*/*/pfx/drive_c/users/*/Documents/"$GAME_DIR"
    # Wineskin / Porting Kit / Sikarugir wrappers
    "$HOME/Applications"/*.app/Contents/SharedSupport/prefix/drive_c/users/*/Documents/"$GAME_DIR"
    "$HOME/Applications"/*/*.app/Contents/SharedSupport/prefix/drive_c/users/*/Documents/"$GAME_DIR"
    /Applications/*.app/Contents/SharedSupport/prefix/drive_c/users/*/Documents/"$GAME_DIR"
    # Plain Wine
    "$HOME/.wine/drive_c/users"/*/Documents/"$GAME_DIR"
)
shopt -u nullglob

# Bottles often link Documents to ~/Documents, so drop duplicates.
dirs=()
seen="|"
for d in "${candidates[@]}"; do
    [ -d "$d" ] || continue
    real=$(cd -P "$d" 2>/dev/null && pwd -P) || continue
    case "$seen" in *"|$real|"*) continue ;; esac
    seen="$seen$real|"
    dirs+=("$real")
done

if [ ${#dirs[@]} -eq 0 ]; then
    say "Could not find the Witcher 3 settings folder."
    say ""
    say "Start the game once (so it creates its settings), quit it,"
    say "and run this again."
    finish 1
fi

set_hdr() {
    local f="$1" tmp
    tmp=$(mktemp) || return 1

    if grep -q '^HdrEnabled=' "$f"; then
        # Replace the value but keep the Windows (CRLF) line ending.
        sed -E "s/^HdrEnabled=[^$CR]*/HdrEnabled=$VALUE/" "$f" > "$tmp"
    elif grep -q '^\[Visuals\]' "$f"; then
        awk -v line="HdrEnabled=$VALUE" '
            { print }
            !done && /^\[Visuals\]/ {
                eol = (substr($0, length($0)) == "\r") ? "\r" : ""
                print line eol
                done = 1
            }' "$f" > "$tmp"
    else
        { cat "$f"; printf '\r\n[Visuals]\r\nHdrEnabled=%s\r\n' "$VALUE"; } > "$tmp"
    fi

    # Write back in place so file permissions and links are kept.
    cat "$tmp" > "$f"
    rm -f "$tmp"
}

changed=0
for d in "${dirs[@]}"; do
    say "Found: $d"
    for name in $CONFIG_FILES; do
        f="$d/$name"
        [ -f "$f" ] || continue
        if [ ! -w "$f" ]; then
            say "   ! $name is read-only, skipped"
            continue
        fi
        if grep -q "^HdrEnabled=$VALUE" "$f"; then
            say "   - $name: HDR already off"
            continue
        fi
        [ -e "$f.before-hdr-off" ] || cp -p "$f" "$f.before-hdr-off"
        if set_hdr "$f"; then
            say "   + $name: HDR turned off"
            changed=$((changed + 1))
        else
            say "   ! $name: could not be changed"
        fi
    done
    say ""
done

if [ $changed -gt 0 ]; then
    say "Done. HDR is now off. Start the game as usual."
    say "Originals were saved next to them as *.before-hdr-off"
else
    say "Nothing to change. HDR was already off."
fi
finish 0
