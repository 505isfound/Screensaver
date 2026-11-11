# Iris Nebula

The Iris Nebula (NGC 7023), 1,400 light years away in the constellation Cepheus.
The picture stays still; the stars twinkle and the nebula glows softly.

## Settings

| Setting | Choices | Default |
|---|---|---|
| Monitors | Automatic, one picture across all monitors, each monitor separately | Automatic |
| Star twinkle | Gentle, Normal, Lively | Normal |
| Nebula glow | Off, Subtle, Stronger | Subtle |
| Slowly zoom in and out | on / off | off |

**Automatic** shows one big picture when your monitors are all the same size and form a neat
rectangle (like a 3 × 2 wall). Otherwise each monitor gets its own view. The nebula is always
centered on the main display (the one Windows marks as "main" in Display settings).

## How it works

- `tools/prepare_image.py` turns the original picture into the files in `assets/`:
  the picture rotated 90° clockwise, a star-free copy, and a list of ~140,000 stars.
- Those files are packed inside `IrisNebula.scr`, so it is a single file.
- `src/shaders.hlsl` does the drawing on the graphics card: each star brightens and dims on its
  own timing, the brightest ones sometimes flash their spikes, and the nebula gas slowly
  "breathes" with a faint wave of light moving outward from the central star.

## Picture credit

NASA, ESA and Digitized Sky Survey 2. Acknowledgment: Davide De Martin (ESA/Hubble).
Source: [ESA/Hubble heic0915b](https://esahubble.org/images/heic0915b/), used under ESA/Hubble's
image terms (CC BY 4.0 with visible credit). The credit is also shown in the screensaver's Settings.
