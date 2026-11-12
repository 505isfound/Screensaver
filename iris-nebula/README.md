# Iris Nebula

The Iris Nebula (NGC 7023), 1,400 light years away in the constellation Cepheus.
The picture stays still; the stars twinkle and the nebula glows softly.

## Settings

| Setting | What it does | Default |
|---|---|---|
| Monitors | Automatic, one picture across all monitors, or each monitor separately | Automatic |
| Picture position | Nebula centered on the main screen, or line up exactly with one of the wallpapers below | centered |
| Twinkle amount / speed | How much and how fast stars twinkle | middle |
| Bursts: how often / brightness | Now and then a random star builds up, pops with a glow and spikes, then settles | middle |
| Glow amount | Strength of all nebula effects (far left = off) | middle |
| Effect speed | Speed of the gas drift, shadows, shimmer, rays and newborn stars | middle |
| Flash: how often / brightness | The nebula's flash and the light wave that follows (far left = never) | middle |
| Nebula effects | Drifting gas, light flash and echo, dust shadows, color shimmer, light rays, newborn stars | all on |
| Storm effects | Lightning, storm cells, churning gas, pulses along the strands, crackle | all on |
| Slowly zoom in and out | Gentle zoom | off |

Every slider shows a percentage. 100% (the middle) is Keegan's chosen look, so each slider can go less or more from there. **Defaults** puts everything back.
All timing is random (nothing repeats on a fixed cycle).

**Automatic** shows one big picture when your monitors are all the same size and form a neat
rectangle (like a 3 × 2 wall). Otherwise each monitor gets its own view. The nebula is always
centered on the main display (the one Windows marks as "main" in Display settings).

## How it works

- `tools/prepare_image.py` turns the original picture into the files in `assets/`:
  the picture rotated 90° clockwise, a star-free copy, and a list of ~140,000 stars.
- Those files are packed inside `IrisNebula.scr`, so it is a single file.
- The first start unpacks the big picture (a few seconds) and saves a ready-to-use copy sized for
  your monitors in `%LOCALAPPDATA%\Iris Nebula Screensaver`. Later starts load that copy, so the
  screensaver appears instantly with the full picture. Changing monitors makes a new copy.
  Deleting that folder is safe.
- `src/shaders.hlsl` does the drawing on the graphics card: each star brightens and dims on its
  own random timing, a few stars give quick bright bursts, the brightest sometimes flash their
  spikes, and the nebula has six effects (see Settings). The nebula's flash is timed randomly
  by `src/main.cpp`.

## Picture credit

NASA, ESA and Digitized Sky Survey 2. Acknowledgment: Davide De Martin (ESA/Hubble).
Source: [ESA/Hubble heic0915b](https://esahubble.org/images/heic0915b/), used under ESA/Hubble's
image terms (CC BY 4.0 with visible credit). The credit is also shown in the screensaver's Settings.

## Wallpapers

Pictures that line up exactly with the screensaver, so the desktop and screensaver match.
Set the wallpaper with **Span** (right-click the desktop → Personalize → Background → pick the
file → "Choose a fit": Span), then pick the same wallpaper under **Picture position** in the
screensaver's Settings. It places the picture the way Windows' Span does (measured on a real desktop).

- [Full picture (10711 × 7712)](wallpapers/iris-nebula-full-10711x7712.jpg)
- [Triple 1080p (5760 × 1080)](wallpapers/iris-nebula-triple-5760x1080.jpg): made for three 1080p monitors side by side.

Same picture credit as above.
