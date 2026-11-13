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

- 2026-11-12: Keegan's look is now the middle (100%) of every slider, not just the default
  position: GetTuning scales from his values. Settings version 2 ignores older saved slider
  positions (their meaning changed).

- 2026-11-12: Five storm effects, each a checkbox (all on): lightning inside the gas (random,
  in busy and quiet spells, often double flashes), storm cells (4 wandering busy areas), churning
  gas (small fast boiling in the bright gas), pulses along the strands, and crackle (tiny jagged
  sparks along gas edges). Settings version 3 switches them on for existing installs.
- 2026-11-12: Fixed dark dots on dimming stars: dimming now covers the whole star (including the
  spikes on spiky stars) evenly, and never removes more than 70% of the star's light.

- 2026-11-12: Star list cleanup (prepare_image.py): dropped stretched smudges and detections that
  sit in a brighter star's glow or spikes (they made dimming spots with no star in the middle).
  143k -> 124k stars.
- 2026-11-12: Dimming capped at 40% of a star's own light (was 70%, looked like "black" stars).
- 2026-11-12: Nebula flash lights the cloud from inside: brighter on thick strands and random hot
  spots, darker where made-up clouds sit in front; a new pattern every flash.
- 2026-11-12: Color shimmer at full shift: red -8%, green +6%, blue +3% (was -10% / +10% / -2%).
- 2026-11-12: Removed light rays, churning gas and pulses along the strands.

- 2026-11-12: Swirling center (checkbox, on) with its own "Swirl speed" slider (25%-400%). The gas
  near the central star turns around it, fastest close in, using the same two-copies trick as the
  drifting gas so it never winds into a smear. Settings version 4 switches it on.

- 2026-11-12: Second round of Keegan's picks became the slider middles (100%): twinkle speed
  74% of before, glow 200%, effect speed 184%, flash brightness 200%, swirl speed 400%. Crackle
  off and slow zoom on by default. Settings version 5 ignores older saved slider positions.

- 2026-11-12: Input in the first 1.5 s is ignored, so the screensaver can be started from a desktop
  shortcut with a hotkey (`IrisNebula.scr /s`) without letting go of the keys closing it.

- 2026-11-12: Four experiments written for demo videos, not switched on in the screensaver
  (no checkbox; IRIS_DEMO_EFFECTS=<bits> turns them on for testing): moving inner light (4096),
  floating dust (8192), falling streams (16384), depth sway (32768). Waiting for Keegan's picks.

- 2026-11-13: Swirling center redone from Keegan's sketch: four layers of wispy gas turning around
  the central star inside the bright core (back layers dim, deep blue, slow; front layers bright,
  bigger, faster, with soft shadows), blended with "screen" so the picture isn't washed out. Each
  layer turns as a whole with a fixed spiral twist, so it never winds up. Same checkbox/slider.
- 2026-11-13: Stars with spikes twinkle in a thin cross shape (center + spikes) instead of a big
  circle, which had made nearby stars light up together (noticed on triple 1080p).
- 2026-11-13: Keegan didn't like the four experiments (inner light, dust, streams, sway); left in
  the code, off, with no checkbox.

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
- 2026-11-12: v0.6.1: slider middles = Keegan's look.
- 2026-11-12: v0.7: storm effects; fixed dark dots on dimming stars.
- 2026-11-12: v0.7.1: cleaner star list, gentler dimming, cloudy flash, less green shimmer, 3 effects removed.
- 2026-11-12: v0.8: swirling center + swirl speed slider.
- 2026-11-12: v0.8.1: Keegan's second set of defaults.
- 2026-11-12: v0.8.2: start from a shortcut/hotkey.
- 2026-11-13: v0.9: layered swirling core; spiky-star twinkle no longer lights up neighbours.
