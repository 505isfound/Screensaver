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

- 2026-11-11: Six nebula effects (drifting gas, light flash + echo, dust shadows, color shimmer,
  light rays, newborn stars). Keegan picked all six: on by default, checkboxes in Settings.
- 2026-11-11: Nebula flash: split-second peak, drops to ~50% almost at once, then fades back over
  a few seconds. Then the light wave travels outward.
- 2026-11-11: No repeating patterns (Keegan dislikes them): all timing uses random noise or random
  events, never fixed cycles. Nebula flash every 26-78 s at random.
- 2026-11-11: Star bursts: a small share of stars flash quickly and drop straight back. About one
  every few seconds per 1080p screen. Twinkle amount unchanged from v0.1.
- 2026-11-11: Settings use sliders (0-100, middle = normal) for twinkle amount/speed, burst
  frequency/brightness, glow amount, effect speed, flash frequency/brightness. Defaults button.

## Changelog
- 2026-11-11: Iris Nebula v0.1.
- 2026-11-11: Added matching triple-monitor wallpaper (5760 × 1080).
- 2026-11-11: Iris Nebula v0.2: instant start with the full picture (cache), no fade-in.
- 2026-11-11: Iris Nebula v0.3: six nebula effects, random timing, star bursts, sliders.
