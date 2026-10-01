# Witcher 3 HDR Off for Mac

Turns off HDR in **The Witcher 3: Wild Hunt** (Next-Gen / Remastered) when you play it on a Mac through **CrossOver, Whisky, Heroic, Porting Kit or Wine**. One double-click, and it works for both the **Steam** and **GOG** versions.

## The problem

On a Mac with an HDR screen (MacBook Pro XDR, Pro Display XDR, HDR monitors), the game turns HDR on by itself. Under CrossOver/Wine the HDR option is missing from the settings menu, so you can't turn it off. You get a washed-out, overblown picture.

## How to use

1. **Quit The Witcher 3.**
2. Download this project: click the green **Code** button, then **Download ZIP**, and unzip it.
3. Double-click **`Witcher3 - Turn HDR Off.command`**.
4. A Terminal window opens, shows what it changed and says **Done**. Start the game.

### "Apple could not verify..." / "unidentified developer"

macOS blocks scripts downloaded from the internet the first time you run them. To allow it:

- **macOS 15 Sequoia and newer:** double-click the file and click **Done** on the warning. Then open **System Settings → Privacy & Security**, scroll down, click **Open Anyway** next to the file name, and confirm.
- **macOS 14 and older:** right-click (or Control-click) the file, choose **Open**, then click **Open** again.

You only need to do this once.

### "You do not have permission to open the application"

The file lost its "can run" flag, which usually happens if you downloaded only the single file and not the ZIP. Open Terminal, type `chmod +x ` (with a space at the end), drag the file into the Terminal window, and press Enter. Then double-click it again.

## What it does

- Looks for the game's settings folder (`Documents/The Witcher 3`) in:
  - your Mac's `~/Documents`
  - all CrossOver bottles
  - all Whisky bottles
  - Heroic prefixes
  - Wineskin, Porting Kit and Sikarugir wrappers
  - `~/.wine`
- In `dx12user.settings` and `user.settings` (and their `.bak` copies), it sets
  ```
  [Visuals]
  HdrEnabled=false
  ```
- Saves a copy of each file it changes as `*.before-hdr-off`.
- Changes nothing else. Your saves and other settings are untouched.

It's a plain shell script, so you can open it in TextEdit and read every line before you run it.

## Notes

- **Run it again after a game update or "Verify files"** if HDR comes back.
- If the picture looks too dark or flat afterwards, adjust **Brightness/Gamma** in the game's display settings.
- **Turning HDR back on:** delete the changed settings file and rename the `.before-hdr-off` copy to the original name. You can also open `dx12user.settings` and change `HdrEnabled=false` to `true`.
- **"Could not find the Witcher 3 settings folder":** start the game once so it creates its settings, quit it, then run the script again. If your bottle is in a custom location, open an issue.

## License

MIT. See [LICENSE](LICENSE).
