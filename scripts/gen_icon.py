# -*- coding: utf-8 -*-
"""RawLab skeuomorphic lens icon generator.

Renders an SVG (gradient-only, no filters — cairosvg compatible) of a
frontal camera lens: knurled metal rim, engraved distance scale, printed
brand double-frame mark, coated glass with cyan/yellow reflections and a
9-blade iris. Outputs the iOS full-bleed master, the macOS rounded tile,
and Android adaptive foregrounds.
"""
import math
import io
import cairosvg
from PIL import Image

YELLOW = "#FFD13A"
CYAN = "#4CDAE9"
VIOLET = "#7C6BFF"


def pt(cx, cy, r, deg):
    a = math.radians(deg)
    return cx + r * math.cos(a), cy + r * math.sin(a)


def arc_path(cx, cy, r, a0, a1, sweep=1):
    x0, y0 = pt(cx, cy, r, a0)
    x1, y1 = pt(cx, cy, r, a1)
    large = 1 if abs(a1 - a0) > 180 else 0
    return f"M {x0:.2f},{y0:.2f} A {r},{r} 0 {large} {sweep} {x1:.2f},{y1:.2f}"


def lens_group(cx, cy, R, with_bg=False, W=1024):
    """All feature sizes are fractions of R so the same art scales."""
    s = []
    add = s.append

    if with_bg:
        add(f'''<defs>
<radialGradient id="bg" cx="0.5" cy="0.42" r="0.75">
  <stop offset="0" stop-color="#2E3036"/>
  <stop offset="0.55" stop-color="#232529"/>
  <stop offset="1" stop-color="#151619"/>
</radialGradient>
<radialGradient id="shadow" cx="0.5" cy="0.5" r="0.5">
  <stop offset="0.62" stop-color="#000000" stop-opacity="0.55"/>
  <stop offset="0.85" stop-color="#000000" stop-opacity="0.25"/>
  <stop offset="1" stop-color="#000000" stop-opacity="0"/>
</radialGradient>
</defs>
<rect width="{W}" height="{W}" fill="url(#bg)"/>
<rect width="{W}" height="{W*0.35}" fill="url(#topsheen)"/>
<ellipse cx="{cx}" cy="{cy+R*0.06:.1f}" rx="{R*1.22:.1f}" ry="{R*1.18:.1f}" fill="url(#shadow)"/>''')

    # ---- defs (unique ids not needed: single group per canvas) ----
    add(f'''<defs>
<linearGradient id="rimMetal" x1="0" y1="0" x2="0.7" y2="1">
  <stop offset="0" stop-color="#9BA0A9"/>
  <stop offset="0.28" stop-color="#565B64"/>
  <stop offset="0.6" stop-color="#22242A"/>
  <stop offset="1" stop-color="#0C0D10"/>
</linearGradient>
<radialGradient id="barrel" cx="0.42" cy="0.36" r="0.75">
  <stop offset="0" stop-color="#33363C"/>
  <stop offset="0.55" stop-color="#17181C"/>
  <stop offset="1" stop-color="#08090B"/>
</radialGradient>
<radialGradient id="bezel" cx="0.4" cy="0.34" r="0.8">
  <stop offset="0" stop-color="#1A1C21"/>
  <stop offset="0.7" stop-color="#0C0D10"/>
  <stop offset="1" stop-color="#050608"/>
</radialGradient>
<radialGradient id="glass" cx="0.5" cy="0.5" r="0.5">
  <stop offset="0" stop-color="#0A0D18"/>
  <stop offset="0.45" stop-color="#101E3E"/>
  <stop offset="0.78" stop-color="#172B52"/>
  <stop offset="1" stop-color="#1D3560"/>
</radialGradient>
<radialGradient id="washCyan"><stop offset="0" stop-color="{CYAN}" stop-opacity="0.5"/><stop offset="1" stop-color="{CYAN}" stop-opacity="0"/></radialGradient>
<radialGradient id="washYellow"><stop offset="0" stop-color="{YELLOW}" stop-opacity="0.55"/><stop offset="1" stop-color="{YELLOW}" stop-opacity="0"/></radialGradient>
<radialGradient id="washViolet"><stop offset="0" stop-color="{VIOLET}" stop-opacity="0.20"/><stop offset="1" stop-color="{VIOLET}" stop-opacity="0"/></radialGradient>
<radialGradient id="flareYellow"><stop offset="0" stop-color="{YELLOW}"/><stop offset="0.55" stop-color="{YELLOW}" stop-opacity="0.5"/><stop offset="1" stop-color="{YELLOW}" stop-opacity="0"/></radialGradient>
<radialGradient id="flareCyan"><stop offset="0" stop-color="{CYAN}"/><stop offset="0.55" stop-color="{CYAN}" stop-opacity="0.45"/><stop offset="1" stop-color="{CYAN}" stop-opacity="0"/></radialGradient>
<radialGradient id="spec"><stop offset="0" stop-color="#FFFFFF" stop-opacity="0.95"/><stop offset="0.55" stop-color="#FFFFFF" stop-opacity="0.35"/><stop offset="1" stop-color="#FFFFFF" stop-opacity="0"/></radialGradient>
<radialGradient id="opening" cx="0.5" cy="0.45" r="0.6">
  <stop offset="0" stop-color="#22355E"/>
  <stop offset="0.55" stop-color="#0D1630"/>
  <stop offset="1" stop-color="#05070E"/>
</radialGradient>
<radialGradient id="bladeBody" gradientUnits="userSpaceOnUse" cx="{cx}" cy="{cy}" r="{R*0.40}">
  <stop offset="0.38" stop-color="#333B4C"/>
  <stop offset="0.62" stop-color="#212734"/>
  <stop offset="1" stop-color="#0D0F16"/>
</radialGradient>
<radialGradient id="bladeBodyDark" gradientUnits="userSpaceOnUse" cx="{cx}" cy="{cy}" r="{R*0.40}">
  <stop offset="0.38" stop-color="#1C212C"/>
  <stop offset="0.62" stop-color="#12151E"/>
  <stop offset="1" stop-color="#0A0C12"/>
</radialGradient>
<radialGradient id="apShade" cx="0.5" cy="0.5" r="0.5">
  <stop offset="0.55" stop-color="#04060A" stop-opacity="0"/>
  <stop offset="0.82" stop-color="#04060A" stop-opacity="0.28"/>
  <stop offset="1" stop-color="#04060A" stop-opacity="0.6"/>
</radialGradient>
<radialGradient id="housing" cx="0.5" cy="0.5" r="0.5">
  <stop offset="0" stop-color="#0B0E15"/>
  <stop offset="1" stop-color="#04050A"/>
</radialGradient>
<clipPath id="glassClip"><circle cx="{cx}" cy="{cy}" r="{R*0.705:.2f}"/></clipPath>
<clipPath id="apClip"><circle cx="{cx}" cy="{cy}" r="{R*0.425:.2f}"/></clipPath>
</defs>''')

    # ---- outer metal rim ----
    add(f'<circle cx="{cx}" cy="{cy}" r="{R:.2f}" fill="url(#rimMetal)"/>')
    # knurling teeth
    n_teeth = 88
    teeth = []
    for i in range(n_teeth):
        a = i * 360.0 / n_teeth
        x0, y0 = pt(cx, cy, R * 0.992, a)
        x1, y1 = pt(cx, cy, R * 0.945, a)
        teeth.append(f'<line x1="{x0:.2f}" y1="{y0:.2f}" x2="{x1:.2f}" y2="{y1:.2f}"/>')
    add(f'<g stroke="#0B0C0E" stroke-opacity="0.6" stroke-width="{R*0.008:.2f}">{"".join(teeth)}</g>')
    # soft top-light on rim + faint bottom bounce
    add(f'<path d="{arc_path(cx, cy, R*0.968, 195, 320)}" fill="none" stroke="#D9DDE4" stroke-opacity="0.35" stroke-width="{R*0.02:.2f}" stroke-linecap="round"/>')
    add(f'<path d="{arc_path(cx, cy, R*0.968, 40, 130)}" fill="none" stroke="#FFFFFF" stroke-opacity="0.10" stroke-width="{R*0.014:.2f}" stroke-linecap="round"/>')

    # ---- barrel body ----
    add(f'<circle cx="{cx}" cy="{cy}" r="{R*0.945:.2f}" fill="url(#barrel)"/>')
    # engraved distance-scale ticks on the top arc
    ticks = []
    for k in range(-5, 6):
        a = -90 + k * 11
        long_tick = (k % 2 == 0)
        r_out = R * 0.912
        r_in = R * 0.868 if long_tick else R * 0.885
        x0, y0 = pt(cx, cy, r_out, a)
        x1, y1 = pt(cx, cy, r_in, a)
        op = 0.85 if long_tick else 0.5
        ticks.append(f'<line x1="{x0:.2f}" y1="{y0:.2f}" x2="{x1:.2f}" y2="{y1:.2f}"/>')
    add(f'<g stroke="#C7CCD4" stroke-opacity="0.65" stroke-width="{R*0.009:.2f}" stroke-linecap="round">{"".join(ticks)}</g>')
    # yellow index mark at 12 o'clock
    x0, y0 = pt(cx, cy, R * 0.916, -90)
    x1, y1 = pt(cx, cy, R * 0.856, -90)
    add(f'<line x1="{x0:.2f}" y1="{y0:.2f}" x2="{x1:.2f}" y2="{y1:.2f}" stroke="{YELLOW}" stroke-width="{R*0.018:.2f}" stroke-linecap="round"/>')
    # machined groove between the scale and the bezel
    add(f'<circle cx="{cx}" cy="{cy}" r="{R*0.845:.2f}" fill="none" stroke="#000000" stroke-opacity="0.55" stroke-width="{R*0.009:.2f}"/>')
    add(f'<circle cx="{cx}" cy="{cy}" r="{R*0.852:.2f}" fill="none" stroke="#FFFFFF" stroke-opacity="0.10" stroke-width="{R*0.004:.2f}"/>')

    # ---- front bezel ----
    add(f'<circle cx="{cx}" cy="{cy}" r="{R*0.795:.2f}" fill="url(#bezel)"/>')
    add(f'<circle cx="{cx}" cy="{cy}" r="{R*0.79:.2f}" fill="none" stroke="#000000" stroke-opacity="0.55" stroke-width="{R*0.012:.2f}"/>')
    # printed brand double-frame mark at the bottom of the bezel (lens-print style)
    mw, mh, ms = R * 0.085, R * 0.062, R * 0.013
    my = cy + R * 0.748
    add(f'''<g transform="translate({cx - R*0.055:.1f},{my:.1f})" fill="none" stroke-linejoin="round">
<rect x="0" y="0" width="{mw:.2f}" height="{mh:.2f}" rx="{ms*0.6:.2f}" stroke="{YELLOW}" stroke-width="{ms:.2f}"/>
<rect x="{R*0.042:.2f}" y="{R*0.033:.2f}" width="{mw:.2f}" height="{mh:.2f}" rx="{ms*0.6:.2f}" stroke="{CYAN}" stroke-width="{ms:.2f}"/>
</g>''')

    # ---- metal lip between bezel and glass ----
    add(f'<circle cx="{cx}" cy="{cy}" r="{R*0.712:.2f}" fill="none" stroke="#B9BEC7" stroke-opacity="0.75" stroke-width="{R*0.008:.2f}"/>')
    add(f'<circle cx="{cx}" cy="{cy}" r="{R*0.700:.2f}" fill="none" stroke="#000000" stroke-opacity="0.6" stroke-width="{R*0.007:.2f}"/>')

    # ---- glass ----
    gr = R * 0.705
    add(f'<circle cx="{cx}" cy="{cy}" r="{gr:.2f}" fill="url(#glass)"/>')
    add(f'<g clip-path="url(#glassClip)">')
    add(f'<ellipse cx="{cx+R*0.25:.1f}" cy="{cy-R*0.32:.1f}" rx="{R*0.62:.1f}" ry="{R*0.62:.1f}" fill="url(#washCyan)"/>')
    add(f'<ellipse cx="{cx-R*0.44:.1f}" cy="{cy+R*0.48:.1f}" rx="{R*0.52:.1f}" ry="{R*0.52:.1f}" fill="url(#washYellow)"/>')
    add(f'<ellipse cx="{cx-R*0.10:.1f}" cy="{cy-R*0.05:.1f}" rx="{R*0.75:.1f}" ry="{R*0.75:.1f}" fill="url(#washViolet)"/>')
    # big specular crescent, upper left (defined core + soft halo)
    add(f'<g transform="rotate(-38 {cx-R*0.28:.1f} {cy-R*0.30:.1f})">'
        f'<ellipse cx="{cx-R*0.28:.1f}" cy="{cy-R*0.30:.1f}" rx="{R*0.44:.1f}" ry="{R*0.13:.1f}" fill="url(#spec)" fill-opacity="0.55"/></g>')
    add(f'<g transform="rotate(-38 {cx-R*0.30:.1f} {cy-R*0.30:.1f})">'
        f'<ellipse cx="{cx-R*0.30:.1f}" cy="{cy-R*0.30:.1f}" rx="{R*0.22:.1f}" ry="{R*0.055:.1f}" fill="url(#spec)"/></g>')
    # small hard sparkle
    add(f'<circle cx="{cx-R*0.42:.1f}" cy="{cy-R*0.44:.1f}" r="{R*0.028:.2f}" fill="#FFFFFF" fill-opacity="0.95"/>')
    add(f'<circle cx="{cx-R*0.42:.1f}" cy="{cy-R*0.44:.1f}" r="{R*0.055:.2f}" fill="url(#spec)"/>')
    # lower bounce reflection near the glass edge
    add(f'<path d="{arc_path(cx, cy, R*0.60, 22, 62)}" fill="none" stroke="#FFFFFF" stroke-opacity="0.14" stroke-width="{R*0.030:.2f}" stroke-linecap="round"/>')
    # coating flares: soft crescent reflections hugging the glass edge
    add(f'<g transform="rotate(55 {cx-0.475*R:.1f} {cy+0.333*R:.1f})">'
        f'<ellipse cx="{cx-0.475*R:.1f}" cy="{cy+0.333*R:.1f}" rx="{R*0.34:.1f}" ry="{R*0.15:.1f}" fill="url(#flareYellow)"/></g>')
    add(f'<g transform="rotate(50 {cx+0.445*R:.1f} {cy-0.356*R:.1f})">'
        f'<ellipse cx="{cx+0.445*R:.1f}" cy="{cy-0.356*R:.1f}" rx="{R*0.32:.1f}" ry="{R*0.14:.1f}" fill="url(#flareCyan)"/></g>')
    # glass edge inner reflection ring
    add(f'<circle cx="{cx}" cy="{cy}" r="{gr*0.94:.2f}" fill="none" stroke="#9FB4D8" stroke-opacity="0.25" stroke-width="{R*0.008:.2f}"/>')
    add('</g>')
    add(f'<circle cx="{cx}" cy="{cy}" r="{gr:.2f}" fill="none" stroke="#000000" stroke-opacity="0.65" stroke-width="{R*0.006:.2f}"/>')

    # ---- iris / aperture ----
    # Twisted-blade iris: each blade is bounded by a straight (slightly bowed)
    # chord on the opening nonagon and a twisted outer arc, so consecutive
    # blades tile the annulus with a spiral shear — reads as a real iris.
    hr = R * 0.40
    add(f'<circle cx="{cx}" cy="{cy}" r="{hr:.2f}" fill="url(#housing)"/>')
    or0 = R * 0.185          # opening radius
    delta = 14.0             # twist (degrees) at the outer radius
    Rout = hr * 1.04

    def bow_chord(a0, a1):
        """Nearly straight chord between two opening vertices, faintly bowed."""
        x0, y0 = pt(cx, cy, or0, a0)
        x1, y1 = pt(cx, cy, or0, a1)
        mx, my = (x0 + x1) / 2, (y0 + y1) / 2
        vx, vy = cx - mx, cy - my
        L = math.hypot(vx, vy) or 1.0
        depth = or0 * 0.045
        qx, qy = mx + vx / L * depth, my + vy / L * depth
        return f"M {x0:.2f},{y0:.2f} Q {qx:.2f},{qy:.2f} {x1:.2f},{y1:.2f}"

    add(f'<g clip-path="url(#apClip)">')
    add(f'<circle cx="{cx}" cy="{cy}" r="{or0:.2f}" fill="url(#opening)"/>')
    add(f'<circle cx="{cx}" cy="{cy}" r="{or0*0.35:.2f}" fill="#FFFFFF" fill-opacity="0.10"/>')
    # faint sheen from the main light falling across the iris
    add(f'<g transform="rotate(-38 {cx-R*0.30:.1f} {cy-R*0.30:.1f})">'
        f'<ellipse cx="{cx-R*0.30:.1f}" cy="{cy-R*0.30:.1f}" rx="{R*0.34:.1f}" ry="{R*0.10:.1f}" fill="#FFFFFF" fill-opacity="0.05"/></g>')
    for i in range(9):
        a = -90 + i * 40.0
        p1x, p1y = pt(cx, cy, or0, a)
        p2x, p2y = pt(cx, cy, or0, a + 40)
        p3x, p3y = pt(cx, cy, Rout, a + 40 + delta)
        p4x, p4y = pt(cx, cy, Rout, a + delta)
        shade = "url(#bladeBody)" if i % 2 == 0 else "url(#bladeBodyDark)"
        d = (f"{bow_chord(a, a + 40)} "
             f"L {p3x:.2f},{p3y:.2f} "
             f"A {Rout:.2f},{Rout:.2f} 0 0 0 {p4x:.2f},{p4y:.2f} Z")
        add(f'<path d="{d}" fill="{shade}"/>')
        add(f'<path d="{bow_chord(a, a + 40)}" fill="none" stroke="#CDD6E6" stroke-opacity="0.6" stroke-width="{R*0.009:.2f}" stroke-linecap="round"/>')
    # blades emerge from shadow toward the housing
    add(f'<circle cx="{cx}" cy="{cy}" r="{hr:.2f}" fill="url(#apShade)"/>')
    add('</g>')
    add(f'<circle cx="{cx}" cy="{cy}" r="{hr:.2f}" fill="none" stroke="#000000" stroke-opacity="0.5" stroke-width="{R*0.006:.2f}"/>')

    return "\n".join(s)


TOPSHEEN = '''<linearGradient id="topsheen" x1="0" y1="0" x2="0" y2="1">
  <stop offset="0" stop-color="#FFFFFF" stop-opacity="0.055"/>
  <stop offset="1" stop-color="#FFFFFF" stop-opacity="0"/>
</linearGradient>'''


def render(svg_body, w, h):
    svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">{svg_body}</svg>'
    png = cairosvg.svg2png(bytestring=svg.encode("utf-8"), output_width=w, output_height=h)
    return Image.open(io.BytesIO(png)).convert("RGBA")


def add_grain(im, sigma=2.0):
    import numpy as np
    arr = np.array(im).astype(np.int16)
    noise = np.random.default_rng(7).normal(0, sigma, arr.shape[:2])[:, :, None]
    arr[:, :, :3] = np.clip(arr[:, :, :3] + noise, 0, 255)
    return Image.fromarray(arr.astype(np.uint8), "RGBA")


def master_1024():
    body = TOPSHEEN + "\n" + lens_group(512, 512, 352, with_bg=True, W=1024)
    im = render(body, 1024, 1024)
    return add_grain(im)


def mac_tile():
    tile = 824
    off = (1024 - tile) // 2
    body = f'''<defs><clipPath id="tile"><rect x="{off}" y="{off}" width="{tile}" height="{tile}" rx="185"/></clipPath></defs>
<g clip-path="url(#tile)">{TOPSHEEN}
<rect width="1024" height="1024" fill="#232529"/>
{lens_group(512, 512, 283, with_bg=True, W=1024)}
</g>'''
    im = render(body, 1024, 1024)
    return add_grain(im)


def android_fg():
    # 108dp canvas, lens R=30dp (content inside the 66dp safe zone)
    scale = 30.0 / 352.0
    inner = lens_group(54, 54, 30, with_bg=False)
    # wrap and rescale: replace absolute coords via group transform on a 1024-local drawing
    body = f'<g transform="translate(54,54) scale({scale:.6f}) translate(-512,-512)">{lens_group(512, 512, 352, with_bg=False)}</g>'
    im = render(body, 108, 108)
    return im


if __name__ == "__main__":
    import os
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "icon-build")
    os.makedirs(out, exist_ok=True)
    m = master_1024()
    m.convert("RGB").save(os.path.join(out, "master-1024.png"))
    mac_tile().save(os.path.join(out, "mac-1024.png"))
    fg = android_fg()
    fg.save(os.path.join(out, "fg-108.png"))
    # small-size readability previews
    prev = Image.new("RGBA", (3 * 160 + 40, 200), (90, 90, 90, 255))
    for i, size in enumerate((128, 64, 48)):
        ic = m.convert("RGB").resize((size, size), Image.LANCZOS)
        prev.paste(ic, (20 + i * 160, 30))
    prev.save(os.path.join(out, "preview-small.png"))
    print("done")
