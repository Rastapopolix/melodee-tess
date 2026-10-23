#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Theo Dainis (melodee-tess)
"""The Tessera demo video from the firmware's own audio and screens.

  build/host/demo_projects video build/video      (tests/demo_projects.c: the clips' WAVs, frames and captions)
  python3 tools/demo_video.py build/video OUT.mp4  (needs Pillow and imageio-ffmpeg, or ffmpeg on the PATH)

Each clip's frames (240 x 240, a frame every 1472 samples: 44100/1472 frames a second) are drawn 3x, nearest, on a
1080 x 1080 card in Tessera's colours with its caption under the screen; a title card, a "More modes" card before
the tour and an end card are added. The audio of every clip is padded to its frames x 1472 samples, so picture and
sound stay together end to end.
"""
import array
import shutil
import subprocess
import sys
import wave
from fractions import Fraction
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FS, SPF = 44100, 1472                                   # samples a frame (demo_projects.c V_BLOCKS x 32)
FPS = Fraction(FS, SPF)
W = H = 1080
BG, SURF, TEXT, MID, GOLD = (13, 19, 38), (24, 33, 60), (246, 240, 224), (179, 178, 176), (236, 184, 72)
MARK = [(44, 64, 120), (58, 84, 150), GOLD, (66, 96, 170), (50, 74, 136), (40, 60, 112), (62, 90, 160), (46, 68, 126),
        (54, 80, 146)]                                  # (the splash's tesserae, firmware/src/splash.c)
FONT = ROOT / "assets/fonts"


def font(weight, size):
    return ImageFont.truetype(str(FONT / f"ChakraPetch-{weight}.ttf"), size)


def wrap(draw, text, f, width):
    lines, line = [], ""
    for word in text.split():
        t = (line + " " + word).strip()
        if draw.textlength(t, font=f) <= width:
            line = t
        else:
            lines.append(line)
            line = word
    return lines + [line] if line else lines


def mark(draw, x, y, size):
    s = size * 0.3
    gap = (size - 3 * s) / 2
    for i, c in enumerate(MARK):
        cx, cy = x + (i % 3) * (s + gap), y + (i // 3) * (s + gap)
        draw.rectangle([cx, cy, cx + s, cy + s], fill=c)


def base():
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    mark(d, 180, 52, 44)
    d.text((240, 56), "TESSERA", font=font("SemiBold", 34), fill=TEXT)
    return im


def card(lines):
    """a still card: the mark large, then (text, weight, size, colour) lines centred"""
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    mark(d, W / 2 - 90, 250, 180)
    y = 500
    for text, weight, size, colour in lines:
        f = font(weight, size)
        d.text(((W - d.textlength(text, font=f)) / 2, y), text, font=f, fill=colour)
        y += int(size * 1.45)
    return im


def caption_layer(text):
    im = base()
    d = ImageDraw.Draw(im)
    d.rectangle([174, 144, 906, 876], fill=SURF)        # (the screen's frame)
    f = font("Medium", 34)
    y = 912
    for line in wrap(d, text, f, 900)[:3]:
        d.text(((W - d.textlength(line, font=f)) / 2, y), line, font=f, fill=TEXT)
        y += 44
    return im


def clips(src):
    """the clips in captions order: (dir, wav, [(t0, t1, text)] in the clip's own time)"""
    rows = [l.split("\t") for l in (src / "captions.txt").read_text(encoding="utf-8").splitlines() if l.strip()]
    out = []
    for name, a, b, text in rows:
        if not out or out[-1][0] != name:
            out.append([name, float(a), []])
        out[-1][2].append((float(a) - out[-1][1], float(b) - out[-1][1], text))
    found = []
    for name, _, caps in out:
        for d in (src / name, src / "a" / name, src / "b" / name):
            if d.is_dir() and d.with_suffix(".wav").exists():
                found.append((d, d.with_suffix(".wav"), caps))
                break
        else:
            raise SystemExit(f"{name}: no frames or WAV in {src}")
    return found


def ffmpeg():
    exe = shutil.which("ffmpeg")
    if exe:
        return exe
    import imageio_ffmpeg
    return imageio_ffmpeg.get_ffmpeg_exe()


def main(src, dst):
    src, dst = Path(src), Path(dst)
    work = dst.with_suffix(".work")
    work.mkdir(parents=True, exist_ok=True)
    audio = array.array("h")
    segments = []                                       # ("card", image, frames) or ("clip", dir, caps, frames)

    def silence(frames):
        audio.extend([0] * (frames * SPF * 2))

    title = card([("TESSERA 2.0", "SemiBold", 86, GOLD), ("Generative firmware for the M-Vave FM-1", "Medium", 40, TEXT),
                  ("Every sound and screen here is the firmware's own", "Regular", 30, MID)])
    more = card([("More modes", "SemiBold", 72, GOLD), ("TURING, EUCLID, MORPH, ATLAS, CONSTELLATION, ANSWER", "Medium", 32, TEXT)])
    end = card([("TESSERA 2.0", "SemiBold", 72, GOLD), ("rastapopolix.github.io/tessera", "Medium", 40, TEXT),
                ("Free, GPL-3.0. Installs from the browser.", "Regular", 30, MID)])
    secs = lambda s: round(s * FS / SPF)
    segments.append(("card", title, secs(3.5)))
    silence(secs(3.5))
    for i, (d, w, caps) in enumerate(clips(src)):
        if d.name.startswith("3-"):
            segments.append(("card", more, secs(2.5)))
            silence(secs(2.5))
        frames = len(list(d.glob("f*.ppm")))
        with wave.open(str(w)) as r:
            a = array.array("h", r.readframes(r.getnframes()))
        need = frames * SPF * 2
        if len(a) > need:
            raise SystemExit(f"{w}: {len(a) // 2} samples, more than {frames} frames x {SPF}")
        a.extend([0] * (need - len(a)))                 # (the last frame's tail: silence, so the clip ends in step)
        audio.extend(a)
        segments.append(("clip", d, caps, frames))
        print(f"{d.name:16s} {frames} frames, {len(a) // 2} samples ({len(a) // 2 == frames * SPF and 'in step'})")
    segments.append(("card", end, secs(4)))
    silence(secs(4))
    total = sum(s[-1] for s in segments)
    assert len(audio) == total * SPF * 2, (len(audio), total)
    wav = work / "audio.wav"
    with wave.open(str(wav), "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(FS)
        w.writeframes(audio.tobytes())
    cmd = [ffmpeg(), "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}",
           "-framerate", f"{FPS.numerator}/{FPS.denominator}", "-i", "-", "-i", str(wav),
           "-c:v", "libx264", "-preset", "medium", "-crf", "18", "-pix_fmt", "yuv420p",
           "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart", str(dst)]
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    n = 0
    for seg in segments:
        if seg[0] == "card":
            raw = seg[1].tobytes()
            for _ in range(seg[2]):
                p.stdin.write(raw)
            n += seg[2]
            continue
        _, d, caps, frames = seg
        layers = {}
        for k in range(frames):
            t = k * SPF / FS
            text = next((c[2] for c in caps if c[0] <= t < c[1]), caps[-1][2])
            if text not in layers:
                layers[text] = caption_layer(text)
            im = layers[text].copy()
            scr = Image.open(d / f"f{k:05d}.ppm").convert("RGB").resize((720, 720), Image.NEAREST)
            im.paste(scr, (180, 150))
            p.stdin.write(im.tobytes())
        n += frames
    p.stdin.close()
    if p.wait():
        raise SystemExit("ffmpeg failed")
    print(f"demo_video: {dst} ({n} frames, {n * SPF / FS:.1f} s)")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
