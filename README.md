# Screensavers

Keegan's Windows screensavers. Each one is its own `.scr` file and works on Windows 10 and 11,
with any monitor setup.

| Screensaver | What it is |
|---|---|
| [Iris Nebula](iris-nebula/) | The Iris Nebula with twinkling stars and a gentle glow |

## Install

1. Download the `.scr` file from the **Releases** page (on the right side of the GitHub page).
2. Move it somewhere it can stay, for example `Documents\Screensavers`.
3. Right-click it and pick **Install** (on Windows 11: **Show more options** → **Install**).
4. Windows opens Screen Saver Settings with it selected. Click **Settings…** to change options.

The first time, Windows may say **"Windows protected your PC"**, because the file isn't signed by a
company. Click **More info** → **Run anyway**.

### Start it any time (shortcut + hotkey)

1. Hold **Shift**, right-click `IrisNebula.scr` → **Copy as path**.
2. Right-click the desktop → **New** → **Shortcut**. Paste the path, then type a space and `/s`
   at the end, for example `"C:\Users\you\Documents\Screensavers\IrisNebula.scr" /s`.
   Click **Next**, name it, click **Finish**.
3. Right-click the new shortcut → **Properties** → click in **Shortcut key** → press the keys you
   want (for example Ctrl + Alt + N) → **OK**.

To remove it, pick another screensaver in Screen Saver Settings and delete the file.

## For developers

Built on GitHub automatically (see `.github/workflows/build.yml`). To build by hand on Linux:

```bash
sudo apt install g++-mingw-w64-x86-64 cmake
cmake -S iris-nebula -B build/iris-nebula -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/iris-nebula
```
