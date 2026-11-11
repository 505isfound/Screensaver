"""
Prepares the Iris Nebula picture for the screensaver.

Input:  the "Large JPEG" of ESA/Hubble image heic0915b
        https://esahubble.org/images/heic0915b/
Output (written to ../assets/):
  iris.jpg        the picture, turned 90 degrees clockwise
  background.png  the same picture with the stars removed, 1/8 size
                  (used to separate stars from gas for twinkling and glow)
  stars.bin       list of stars found in the picture (see STAR FORMAT below)

Run:  python prepare_image.py path/to/heic0915b.jpg
Needs: pip install numpy opencv-python-headless

STAR FORMAT: little-endian, header "STAR" + uint32 count, then per star
  float32 x, y        position in iris.jpg pixels
  float32 radius      rough star radius in pixels
  float32 brightness  0..1 peak brightness above the background
"""
import struct
import sys
from pathlib import Path

import cv2
import numpy as np

MAX_STARS = 150000
BG_SCALE = 8

def main(src):
    out = Path(__file__).resolve().parent.parent / "assets"
    out.mkdir(exist_ok=True)

    img = cv2.imread(src, cv2.IMREAD_COLOR)
    img = cv2.rotate(img, cv2.ROTATE_90_CLOCKWISE)
    h, w = img.shape[:2]
    print("rotated size", w, h, "(update kImageWidth/kImageHeight in src/main.cpp if this changes)")
    cv2.imwrite(str(out / "iris.jpg"), img, [cv2.IMWRITE_JPEG_QUALITY, 93])

    # Starless background: shrink, remove small bright dots, smooth.
    small = cv2.resize(img, (w // 4, h // 4), interpolation=cv2.INTER_AREA)
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (15, 15))
    bg = cv2.morphologyEx(small, cv2.MORPH_OPEN, kernel)
    bg = cv2.medianBlur(bg, 15)
    bg = cv2.GaussianBlur(bg, (0, 0), 2)
    bg = cv2.resize(bg, (w // BG_SCALE, h // BG_SCALE), interpolation=cv2.INTER_AREA)
    cv2.imwrite(str(out / "background.png"), bg)

    # Stars: local peaks well above the background.
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY).astype(np.float32)
    bg_full = cv2.resize(cv2.cvtColor(bg, cv2.COLOR_BGR2GRAY), (w, h),
                         interpolation=cv2.INTER_LINEAR).astype(np.float32)
    star = cv2.GaussianBlur(gray - bg_full, (0, 0), 1.0)
    # "sharpness": small dots stand out from their surroundings, gas does not
    dog = cv2.GaussianBlur(gray, (0, 0), 1.0) - cv2.GaussianBlur(gray, (0, 0), 6.0)
    peaks = ((dog == cv2.dilate(dog, np.ones((7, 7), np.uint8)))
             & (dog > 18) & (star > 30))
    ys, xs = np.nonzero(peaks)
    vals = star[ys, xs]
    order = np.argsort(vals)[::-1][:MAX_STARS]
    ys, xs, vals = ys[order], xs[order], vals[order]
    print("stars kept", len(xs), "faintest", vals[-1])

    recs = []
    for x, y, v in zip(xs, ys, vals):
        # radius: how far the star stays above half its peak (scan 4 directions)
        r = 0
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            k = 1
            while k < 60:
                px, py = x + dx * k, y + dy * k
                if not (0 <= px < w and 0 <= py < h) or star[py, px] < 0.5 * v:
                    break
                k += 1
            r = max(r, k)
        if r > 25:
            continue  # big soft blob, probably gas, not a star
        recs.append(struct.pack("<4f", float(x), float(y), float(r), min(v / 255.0, 1.0)))
    with open(out / "stars.bin", "wb") as f:
        f.write(b"STAR" + struct.pack("<I", len(recs)) + b"".join(recs))
    print("done")

if __name__ == "__main__":
    main(sys.argv[1])
