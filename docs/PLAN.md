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

- 2026-11-11: Nebula effects use grid-free "gradient noise" (turned per layer, slightly bent).
  The earlier grid-based noise showed up as soft squares in the dust shadows on a big wall.

- 2026-11-12: Star bursts redesigned as a ~2.4 s event: 1 s build-up (brighter, flicker speeding
  up), short dip, then a pop with a big glow, four spikes lined up with the picture's own star
  spikes, an expanding ring, and an afterglow. No twinkling during a burst. Rarer: about one every
  8-10 s per 1080p screen at the normal setting.

- 2026-11-12: Version number shown in the Settings window title (src/version.h). Work now goes to
  main, so each update appears on the Releases page as "Latest build".

- 2026-11-12: Removed the ring from star bursts (Keegan: not wanted).

- 2026-11-12: "Picture position" setting: nebula centered on the main screen (default), or match
  a wallpaper exactly. Matching copies Windows' Span math (fill the box around all monitors,
  keep the shape, crop equally). Wallpapers listed in settings.cpp (kWallpapers) with the part of
  iris.jpg each shows. Keegan's home: 5120x1440 ultrawide + 2560x1440 centered above, Full picture.

- 2026-11-12: Nebula flash rebuilt like the star bursts: 1.2 s build-up with speeding-up flicker,
  short dip, then a pop with a glow and four spikes that fades within about half a second
  (about a quarter left after 0.4 s), followed by a soft wave of light. Replaces the old long tail.

- 2026-11-12: Each slider shows a percentage compared with normal (100% = normal; speeds go
  25%-400%, "never"/"off" at the far left where that applies).

- 2026-11-12: Measured from Keegan's home desktop screenshot: Windows' Span cuts 1/3 of the extra
  height from the top and 2/3 from the bottom (not centered). Match mode now does the same.

- 2026-11-12: Defaults set to Keegan's picks: one picture across all monitors, match wallpaper Full
  picture, twinkle 200% / speed 200%, bursts 14% often / 200% bright, glow 100%, effect speed
  250%, flash 100% / 100%. Percent labels still measure against the slider middle.
- 2026-11-12: Nebula flash: no cross/spikes. Color shimmer only goes toward teal, never purple.

## Changelog
- 2026-11-11: Iris Nebula v0.1.
- 2026-11-11: Added matching triple-monitor wallpaper (5760 × 1080).
- 2026-11-11: Iris Nebula v0.2: instant start with the full picture (cache), no fade-in.
- 2026-11-11: Iris Nebula v0.3: six nebula effects, random timing, star bursts, sliders.
- 2026-11-11: v0.3.1: fixed square-looking dust shadows.
- 2026-11-12: v0.4: new star bursts.
- 2026-11-12: v0.4.1: no ring on star bursts.
- 2026-11-12: v0.5: Picture position / match wallpaper; full-picture wallpaper added.
- 2026-11-12: v0.5.1: nebula flash like the star bursts.
- 2026-11-12: v0.5.2: slider percentages; fixed overlapping effect checkboxes.
- 2026-11-12: v0.5.3: match-wallpaper placement corrected to Windows' real Span behaviour.
- 2026-11-12: v0.6: Keegan's defaults; no cross on nebula flash; no purple shimmer.
