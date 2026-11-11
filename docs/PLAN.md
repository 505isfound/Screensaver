# Plan

## Decisions
- 2026-11-11: Separate native Windows screensavers (`.scr`), not part of Monad. One folder and one
  `.scr` per screensaver.
- 2026-11-11: C++ with Direct3D 11 (built into Windows 10/11). Built on GitHub with mingw-w64, so
  the `.scr` is one file with no extra DLLs. Pictures are packed inside the file.
- 2026-11-11: Monitor layout "Automatic": one picture across all monitors when they are the same
  size and form a neat rectangle, otherwise one view per monitor. Nebula centered on the main
  display. Same look on any screen: 1728 picture pixels across the main screen's short side.
- 2026-11-11: Iris Nebula picture: ESA/Hubble heic0915b, rotated 90° clockwise to match Keegan's
  copy, framed "zoomed out". Picture stays still by default; liveness comes mostly from star
  twinkle, plus a subtle nebula glow. Slow zoom is an optional setting (off by default).

- 2026-11-11: Instant start: the screensaver window stays hidden until the first frame is ready
  (no black screen, no fade). A ready-made copy of the sized picture and the compiled shaders are
  cached in %LOCALAPPDATA%\Iris Nebula Screensaver.

## Changelog
- 2026-11-11: Iris Nebula v0.1.
- 2026-11-11: Added matching triple-monitor wallpaper (5760 × 1080).
- 2026-11-11: Iris Nebula v0.2: instant start with the full picture (cache), no fade-in.
