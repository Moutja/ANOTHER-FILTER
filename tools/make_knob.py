"""Renders the knob filmstrips used by resource/ms20.uidesc (needs: pip install pillow).
   Outputs knob.png (64 px frames) and knob_s.png (48 px frames) in resource/, each with a _2.0x twin for HiDPI.
   Frames are stacked vertically, FRAMES of them, value 0.0 at the top frame -> 1.0 at the bottom."""
from PIL import Image, ImageDraw
import math, os

FRAMES = 101
ACCENT = (255, 138, 31, 255)
TRACK = (58, 63, 72, 255)
POINTER = (230, 227, 220, 255)
BODY_TOP = (52, 56, 64, 255)
BODY_BOT = (30, 32, 38, 255)
RIM = (12, 13, 16, 255)
SS = 4  # supersampling factor for smooth edges

def frame(size, value):
    S = size * SS
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = S / 2
    u = S / 64.0  # everything below is designed on a 64 px grid

    def pt(r, deg):  # deg: PIL convention (clockwise from 3 o'clock, y down)
        a = math.radians(deg)
        return c + r * math.cos(a), c + r * math.sin(a)

    # arc track + value arc (start at 135 deg = bottom-left, sweep 270 deg clockwise)
    r_arc, w = 28.5 * u, 4.0 * u
    box = [c - r_arc, c - r_arc, c + r_arc, c + r_arc]
    start, sweep = 135.0, 270.0
    d.arc(box, start, start + sweep, fill=TRACK, width=int(w))
    for ang in (start, start + sweep):
        x, y = pt(r_arc - w / 2 + 0.5 * u, ang)
        d.ellipse([x - w / 2, y - w / 2, x + w / 2, y + w / 2], fill=TRACK)
    if value > 0.002:
        end = start + sweep * value
        d.arc(box, start, end, fill=ACCENT, width=int(w))
        for ang in (start, end):
            x, y = pt(r_arc - w / 2 + 0.5 * u, ang)
            d.ellipse([x - w / 2, y - w / 2, x + w / 2, y + w / 2], fill=ACCENT)

    # knob body: vertical gradient disc with a dark rim
    r_body = 21 * u
    body = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    bd = ImageDraw.Draw(body)
    for y in range(int(c - r_body), int(c + r_body) + 1):
        t = (y - (c - r_body)) / (2 * r_body)
        col = tuple(int(BODY_TOP[i] + (BODY_BOT[i] - BODY_TOP[i]) * t) for i in range(3)) + (255,)
        bd.line([(0, y), (S, y)], fill=col)
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).ellipse([c - r_body, c - r_body, c + r_body, c + r_body], fill=255)
    img.paste(body, (0, 0), mask)
    d.ellipse([c - r_body, c - r_body, c + r_body, c + r_body], outline=RIM, width=int(1.5 * u))

    # pointer
    ang = start + sweep * value
    x1, y1 = pt(8 * u, ang)
    x2, y2 = pt(18 * u, ang)
    pw = 3.0 * u
    d.line([(x1, y1), (x2, y2)], fill=POINTER, width=int(pw))
    for x, y in ((x1, y1), (x2, y2)):
        d.ellipse([x - pw / 2, y - pw / 2, x + pw / 2, y + pw / 2], fill=POINTER)

    return img.resize((size, size), Image.LANCZOS)

def strip(size):
    out = Image.new("RGBA", (size, size * FRAMES), (0, 0, 0, 0))
    for i in range(FRAMES):
        out.paste(frame(size, i / (FRAMES - 1)), (0, i * size))
    return out

if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    res = os.path.join(here, "..", "resource")
    for name, size in (("knob", 64), ("knob_s", 48)):
        strip(size).save(os.path.join(res, name + ".png"), optimize=True)
        strip(size * 2).save(os.path.join(res, name + "_2.0x.png"), optimize=True)
    print("wrote knob / knob_s filmstrips (1x and 2x),", FRAMES, "frames each")
