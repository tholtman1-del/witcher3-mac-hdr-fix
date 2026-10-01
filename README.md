# Witcher 3 HDR Fix for Mac

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/F2J527ZLUN)

Turns off the washed-out HDR in **The Witcher 3: Wild Hunt** (Next-Gen / Remastered) when you play it on a Mac with **CrossOver** or **Whisky**. Works for the **Steam** and **GOG** versions. You install it with one double-click.

## The problem

On a Mac with an HDR screen (MacBook Pro with a Liquid Retina XDR display, Pro Display XDR, HDR monitors), CrossOver tells the game that the screen is an HDR10 display. The game then **always** renders in HDR:

- The picture looks washed out and overblown.
- The settings menu has no HDR option.
- Setting `HdrEnabled=false` in `dx12user.settings` does **not** help. The game ignores it on a screen that reports HDR, and writes it back.

## The fix

A small `dinput8.dll` goes next to `witcher3.exe`. When the game asks what kind of screen it has, the DLL answers **"SDR"**, the same answer Windows gives when "Use HDR" is switched off. The game then picks normal SDR output by itself.

- Nothing in the game files is changed. Your saves and settings are untouched.
- Input still works normally: the DLL passes everything else on to the real `dinput8.dll`.
- It works alongside the [Witcher 3 5.00b black-screen fix for CrossOver](https://github.com/tholtman1-del/witcher3-crossover-fix). The two use different DLLs.

## Install

1. **Quit The Witcher 3.**
2. Download this project: click the green **Code** button, then **Download ZIP**, and unzip it.
3. Double-click **`Install HDR Fix.command`**.
4. It finds the game in your CrossOver and Whisky bottles and installs the fix. When it says **Done**, start the game as usual.

If the game isn't found automatically, the installer asks you to drag the game folder into its window.

### "Apple could not verify..." / "unidentified developer"

macOS blocks scripts downloaded from the internet the first time you run them. To allow it:

- **macOS 15 Sequoia and newer:** double-click the file and click **Done** on the warning. Then open **System Settings → Privacy & Security**, scroll down, click **Open Anyway** next to the file name, and confirm.
- **macOS 14 and older:** right-click (or Control-click) the file, choose **Open**, then click **Open** again.

You only need to do this once.

## Check that it works

The fix writes a log to `witcher3-hdrfix.log` in the bottle's user folder. For CrossOver, that's:

```
~/Library/Application Support/CrossOver/Bottles/<bottle name>/drive_c/users/crossover/witcher3-hdrfix.log
```

After the game reaches the main menu, the last line should be:

```
game set colour space 0 (SDR) -> RESULT: HDR is OFF
```

You may also see one `HDR is ON` line a moment earlier, right at startup. That's the game applying its saved HDR setting before it checks the screen. It switches to SDR within a second or two.

## Uninstall

Quit the game and double-click **`Uninstall HDR Fix.command`**. It removes the DLL and the Wine setting. If you had a different `dinput8.dll` from another mod before installing, it puts that one back.

## What the installer does

For each Witcher 3 installation it finds (`bin/x64_dx12/witcher3.exe`):

1. Copies `dinput8.dll` into `bin/x64_dx12`. Any existing `dinput8.dll` from another mod is kept as `dinput8.dll.before-hdrfix`.
2. Adds a Wine DLL override for **`witcher3.exe` only**: `dinput8 = native,builtin`. Without it, Wine would use its own built-in dinput8 and ignore the DLL. Other programs in the bottle are not affected.

**Other Wine setups** (Heroic, Porting Kit, plain Wine): copy `dinput8.dll` into `bin/x64_dx12` yourself. Then, in Wine configuration under **Libraries**, add `dinput8` and set it to **Native then Builtin**.

## Notes

- **After a game update or "Verify files", run the installer again** if HDR comes back.
- To get HDR back for a session without uninstalling, launch the game with the environment variable `W3HDRFIX_ALLOW_HDR=1`.
- Tested on macOS 27, CrossOver 26.3 (D3DMetal), GOG version 5.00b, DX12, on a MacBook Pro with an XDR display.

## Build it yourself

`dinput8.dll` is built from [`src/hdrfix.c`](src/hdrfix.c) with MinGW-w64 (`brew install mingw-w64`):

```sh
x86_64-w64-mingw32-gcc -shared -O2 -s -static-libgcc -o dinput8.dll src/hdrfix.c src/dinput8.def
```

SHA-256 of the released `dinput8.dll`:
`e787fd942c50e7754005831ecab34f0369e6344c6a2f78bf4afb58fa248d4356`

## License

MIT. See [LICENSE](LICENSE).
