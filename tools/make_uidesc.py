"""Generates resource/ms20.uidesc (the plugin window layout). Edit the numbers below and re-run:
       python tools/make_uidesc.py
   No dependencies. The layout() function is also used by the optional preview script."""
import os

TAGS = ["HpFreq", "HpPeak", "LpFreq", "LpPeak", "Drive", "DryWet", "Output",
        "Osc1Wave", "Osc1Oct", "Osc1Pitch", "Osc1Shape",
        "Osc2Wave", "Osc2Oct", "Osc2Pitch", "Osc2Shape",
        "XMod", "SyncRing", "OscMix",
        "AmpA", "AmpD", "AmpS", "AmpR",
        "DriveType",
        "FEnvA", "FEnvD", "FEnvS", "FEnvR", "FEnvAmt",
        "LfoRate", "LfoWave", "LfoDepth", "LfoTarget",
        "RevMix", "RevSize", "RevDamp",
        "ModWheel", "PitchBend",    # driven by MIDI, no on-screen control
        "Preset"]            # index == parameter ID

W, H = 1056, 558
BIG, SMALL = 64, 48        # knob sizes
COL_BIG, COL_SMALL = 80, 64

# (group label, x, [(column label, tag), ...], knob kind)
ROW1_Y, ROW2_Y, ROW3_Y, ROW4_Y = 14, 140, 296, 440   # section label y of each row
OSC_X = 282
row1 = [
    ("OSC 1",     OSC_X,       [("WAVE", "Osc1Wave"), ("OCT", "Osc1Oct"), ("PITCH", "Osc1Pitch"), ("SHAPE", "Osc1Shape")]),
    ("OSC 2",     OSC_X + 276, [("WAVE", "Osc2Wave"), ("OCT", "Osc2Oct"), ("PITCH", "Osc2Pitch"), ("SHAPE", "Osc2Shape")]),
    ("MOD / MIX", OSC_X + 552, [("X-MOD", "XMod"), ("SYNC/RING", "SyncRing"), ("MIX", "OscMix")]),
]
row2 = [
    ("HIGH-PASS", 30,  [("HP FREQ", "HpFreq"), ("HP PEAK", "HpPeak")], BIG),
    ("LOW-PASS",  220, [("LP FREQ", "LpFreq"), ("LP PEAK", "LpPeak")], BIG),
    ("OUTPUT",    410, [("DRIVE", "Drive"), ("DRIVE TYPE", "DriveType"), ("DRY/WET", "DryWet"), ("OUTPUT", "Output")], BIG),
    ("AMP ENV",   770, [("ATTACK", "AmpA"), ("DECAY", "AmpD"), ("SUSTAIN", "AmpS"), ("RELEASE", "AmpR")], SMALL),
]

row3 = [
    ("FILTER ENV", 30,  [("ATTACK", "FEnvA"), ("DECAY", "FEnvD"), ("SUSTAIN", "FEnvS"), ("RELEASE", "FEnvR"), ("AMOUNT", "FEnvAmt")]),
    ("LFO",        410, [("RATE", "LfoRate"), ("WAVE", "LfoWave"), ("DEPTH", "LfoDepth"), ("TARGET", "LfoTarget")]),
    ("REVERB",     770, [("MIX", "RevMix"), ("SIZE", "RevSize"), ("DAMP", "RevDamp")]),
]

def layout():
    """Returns a list of dicts: kind(label|line|knob|value|title), x, y, w, h, text, tag, size."""
    items = [dict(kind="title", x=30, y=10, w=240, h=30, text="ANOTHER FILTER"),
             dict(kind="sub",   x=30, y=44, w=220, h=14, text="SYNTH + MS-20 STYLE FILTER")]
    def group(name, x, cols, size, y0, colw, in_big_row=False):
        w = len(cols) * colw
        items.append(dict(kind="section", x=x, y=y0, w=w, h=16, text=name))
        items.append(dict(kind="line", x=x, y=y0 + 18, w=w, h=1))
        ky = y0 + 18 + 10 + 14 + 4                      # below the column labels
        for i, (label, tag) in enumerate(cols):
            cx = x + i * colw
            items.append(dict(kind="label", x=cx, y=y0 + 28, w=colw, h=14, text=label, small=(size == SMALL)))
            if in_big_row:   # small knobs sharing a row with big ones are centred on them
                kyy, vy = ky + (BIG - size) // 2, ky + BIG + 6
            else:
                kyy, vy = ky, ky + size + 6
            items.append(dict(kind="knob", x=cx + (colw - size) // 2, y=kyy, w=size, h=size, tag=tag, size=size))
            items.append(dict(kind="value", x=cx + 2, y=vy, w=colw - 4, h=18 if size == BIG else 16, tag=tag, small=(size == SMALL)))
    for name, x, cols in row1:
        group(name, x, cols, SMALL, ROW1_Y, COL_SMALL)
    for name, x, cols, size in row2:
        group(name, x, cols, size, ROW2_Y, COL_BIG if size == BIG else COL_SMALL, in_big_row=True)
    for name, x, cols in row3:
        group(name, x, cols, SMALL, ROW3_Y, COL_SMALL)

    # oscilloscope strip along the bottom (drawn by ScopeView in ms20_controller.cpp)
    items.append(dict(kind="section", x=30, y=ROW4_Y, w=996, h=16, text="SCOPE"))
    items.append(dict(kind="line", x=30, y=ROW4_Y + 18, w=996, h=1))
    items.append(dict(kind="scope", x=30, y=ROW4_Y + 28, w=996, h=76))

    # preset selector under the title
    items.append(dict(kind="label", x=30, y=70, w=64, h=14, text="PRESET", small=True))
    items.append(dict(kind="knob", x=38, y=86, w=SMALL, h=SMALL, tag="Preset", size=SMALL))
    items.append(dict(kind="value", x=100, y=99, w=156, h=22, tag="Preset", small=False))
    return items

T = "~ TransparentCColor"

def view_xml(it):
    k = it["kind"]
    o = f'origin="{it["x"]}, {it["y"]}" size="{it["w"]}, {it["h"]}"'
    if k == "title":
        return f'<view class="CTextLabel" {o} title="{it["text"]}" font="title" font-color="accent" text-alignment="left" back-color="{T}" frame-color="{T}"/>'
    if k == "sub":
        return f'<view class="CTextLabel" {o} title="{it["text"]}" font="section" font-color="dim" text-alignment="left" back-color="{T}" frame-color="{T}"/>'
    if k == "section":
        return f'<view class="CTextLabel" {o} title="{it["text"]}" font="section" font-color="dim" text-alignment="left" back-color="{T}" frame-color="{T}"/>'
    if k == "line":
        return f'<view class="CViewContainer" {o} background-color="line" transparent="false"/>'
    if k == "label":
        font = "labelsmall" if it.get("small") else "label"
        return f'<view class="CTextLabel" {o} title="{it["text"]}" font="{font}" font-color="text" text-alignment="center" back-color="{T}" frame-color="{T}"/>'
    if k == "scope":
        return f'<view class="CView" {o} custom-view-name="Scope"/>'
    if k == "knob":
        bmp = "knob_s" if it["size"] == SMALL else "knob"
        return (f'<view class="CAnimKnob" {o} control-tag="{it["tag"]}" bitmap="{bmp}" '
                f'height-of-one-image="{it["size"]}" sub-pixmaps="101" mode="linear mode"/>')
    if k == "value":
        font = "valuesmall" if it.get("small") else "value"
        return (f'<view class="CParamDisplay" {o} control-tag="{it["tag"]}" font="{font}" font-color="text" '
                f'text-alignment="center" back-color="panel" frame-color="line" round-rect-radius="4"/>')
    raise ValueError(k)

def build():
    views = "\n".join("    " + view_xml(i) for i in layout())
    tags = "\n".join(f'    <control-tag name="{n}" tag="{i}"/>' for i, n in enumerate(TAGS))
    return f'''<?xml version="1.0" encoding="UTF-8"?>
<vstgui-ui-description version="1">
  <bitmaps>
    <bitmap name="knob" path="knob.png"/>
    <bitmap name="knob_s" path="knob_s.png"/>
  </bitmaps>
  <colors>
    <color name="bg" rgba="#15171bff"/>
    <color name="panel" rgba="#1e2127ff"/>
    <color name="line" rgba="#33373fff"/>
    <color name="accent" rgba="#ff8a1fff"/>
    <color name="text" rgba="#e6e3dcff"/>
    <color name="dim" rgba="#8b919aff"/>
  </colors>
  <fonts>
    <font name="title" font-name="Arial" size="20" bold="true" italic="false" underline="false" strike-through="false"/>
    <font name="section" font-name="Arial" size="10" bold="true" italic="false" underline="false" strike-through="false"/>
    <font name="label" font-name="Arial" size="10" bold="true" italic="false" underline="false" strike-through="false"/>
    <font name="labelsmall" font-name="Arial" size="9" bold="true" italic="false" underline="false" strike-through="false"/>
    <font name="value" font-name="Arial" size="11" bold="false" italic="false" underline="false" strike-through="false"/>
    <font name="valuesmall" font-name="Arial" size="10" bold="false" italic="false" underline="false" strike-through="false"/>
  </fonts>
  <control-tags>
{tags}
  </control-tags>
  <template name="view" class="CViewContainer" size="{W}, {H}" background-color="bg" transparent="false">
{views}
  </template>
</vstgui-ui-description>
'''

if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "resource", "ms20.uidesc")
    open(out, "w").write(build())
    print("wrote", os.path.normpath(out))
