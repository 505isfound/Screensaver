# Notes for Claude

Keegan's Windows screensavers. Claude writes all the code. Keep it human-readable.

## Working style
- Keegan is not a programmer. Explain things at a beginner level, short and casual.
- Avoid command-line steps for him. He downloads the `.scr` from Releases and right-clicks Install.
- When he asks "can we do X?", discuss options before building.

## Rules
- Each screensaver is separate: its own folder and its own `.scr`. Never bundle themes together.
- Must work on any Windows 10/11 PC and any monitor layout (see `iris-nebula/src/layout.cpp`).
- Single self-contained `.scr` file, no installers or extra DLLs.
- Only use pictures we're allowed to use, and show the required credit in Settings and the README.
- Defaults should be calm. Extra motion goes behind a setting.

## Keeping track
- Update `docs/PLAN.md` (decisions + changelog) whenever something changes.
- Ideas go in `IDEAS.md`; move finished ones to **Done**.

## Testing without Windows
The build works on Linux with mingw-w64. Wine + Xvfb can run it for screenshots
(`wine IrisNebula.scr /s`). Wine's shader compiler is stricter than Windows', which is why the star
list is stored in a texture and stars are drawn without instancing.
