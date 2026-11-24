"""Render Doomgazer app / plugin icon PNGs and design previews."""

from __future__ import annotations

import argparse
import math
from pathlib import Path
from typing import Callable

from PIL import Image, ImageDraw, ImageFont

INK = (0x10, 0x12, 0x16)
PANEL = (0x18, 0x1C, 0x24)
LINE = (0x2E, 0x35, 0x42)
TEXT = (0xE6, 0xE2, 0xD8)
AMBER = (0xE0, 0x9A, 0x45)
ACID = (0x86, 0xC4, 0x78)
CYAN = (0x6A, 0xC4, 0xCE)
DIM = (0x8A, 0x93, 0xA1)
WHITE = (0xFA, 0xF8, 0xF5)
BLACK = (0x00, 0x00, 0x00)

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "plugin" / "assets"
PREVIEW_DIR = OUT / "icon_previews"

DrawFn = Callable[[ImageDraw.ImageDraw, int], None]


def rounded_tile(img: Image.Image, size: int) -> ImageDraw.ImageDraw:
    margin = max(1, size // 16)
    radius = max(2, size // 5)
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle(
        (margin, margin, size - margin - 1, size - margin - 1),
        radius=radius,
        fill=INK + (255,),
        outline=LINE + (255,),
        width=max(1, size // 64),
    )
    return draw


def render(size: int, motif: DrawFn) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = rounded_tile(img, size)
    motif(draw, size)
    return img


def idea_01_horizon_image(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    rounded_tile(img, size)
    draw = ImageDraw.Draw(img)
    cy = size * 0.58
    slit_h = max(2, size // 48)
    pad = size * 0.22
    draw.rectangle((pad, cy - slit_h // 2, size - pad, cy + slit_h // 2), fill=AMBER + (255,))

    glow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    steps = max(6, size // 28)
    gx0, gx1 = int(size * 0.18), int(size * 0.82)
    for i in range(steps):
        t = i / (steps - 1)
        alpha = int(110 * (1.0 - t))
        band_h = max(1, int(size * 0.035))
        y1 = int(cy - slit_h - i * band_h * 0.85)
        y0 = y1 - band_h
        gd.rectangle((gx0, y0, gx1, y1), fill=ACID + (alpha,))
    img = Image.alpha_composite(img, glow)
    return img


def idea_04_monogram(draw: ImageDraw.ImageDraw, size: int) -> None:
    cx, cy = size * 0.5, size * 0.52
    thick = max(3, size // 8)
    outer = size * 0.36
    # D stem
    x0 = cx - outer
    draw.rounded_rectangle(
        (x0, cy - outer, x0 + thick, cy + outer),
        radius=max(1, thick // 3),
        fill=TEXT + (255,),
    )
    # D bowl
    draw.pieslice(
        (cx - outer, cy - outer, cx + outer * 0.65, cy + outer),
        start=270,
        end=90,
        fill=TEXT + (255,),
    )
    draw.rectangle((cx - outer + thick - 1, cy - outer + thick, cx, cy + outer - thick), fill=INK + (255,))
    # amber slit crossbar
    slit_y = cy - thick * 0.15
    slit_h = max(2, size // 40)
    draw.rectangle(
        (x0 + thick * 0.4, slit_y, cx + outer * 0.35, slit_y + slit_h),
        fill=AMBER + (255,),
    )


def idea_06_pedal(draw: ImageDraw.ImageDraw, size: int) -> None:
    m = size * 0.16
    body = (m, m * 1.35, size - m, size - m * 0.85)
    r = max(3, size // 12)
    draw.rounded_rectangle(body, radius=r, fill=PANEL + (255,), outline=LINE + (255,), width=max(1, size // 80))
    inner = (
        body[0] + size * 0.06,
        body[1] + size * 0.1,
        body[2] - size * 0.06,
        body[3] - size * 0.08,
    )
    draw.rounded_rectangle(inner, radius=max(2, r // 2), fill=INK + (255,))
    led_r = max(2, size // 22)
    lx = (body[0] + body[2]) * 0.5
    ly = body[1] + size * 0.14
    draw.ellipse((lx - led_r, ly - led_r, lx + led_r, ly + led_r), fill=AMBER + (255,))
    draw.ellipse(
        (lx - led_r * 0.35, ly - led_r * 0.4, lx - led_r * 0.05, ly - led_r * 0.1),
        fill=(0xFF, 0xD8, 0x9A, 180),
    )
    foot_w = size * 0.18
    foot_h = max(2, size // 32)
    draw.rounded_rectangle(
        ((size - foot_w) * 0.5, body[3] - foot_h * 0.3, (size + foot_w) * 0.5, body[3] + foot_h),
        radius=1,
        fill=DIM + (200,),
    )


def idea_08_shroud(draw: ImageDraw.ImageDraw, size: int) -> None:
    base_y = size * 0.72
    gap = size * 0.08
    bar_w = max(3, size // 14)
    heights = (0.38, 0.52, 0.44)
    total_w = bar_w * 3 + gap * 2
    x0 = (size - total_w) * 0.5
    for i, frac in enumerate(heights):
        x = x0 + i * (bar_w + gap)
        top = base_y - size * frac
        draw.rounded_rectangle(
            (x, top, x + bar_w, base_y),
            radius=max(1, bar_w // 3),
            fill=INK + (255,),
            outline=ACID + (220,),
            width=max(1, size // 96),
        )


def white_tile(img: Image.Image, size: int) -> ImageDraw.ImageDraw:
    margin = max(1, size // 16)
    radius = max(2, size // 5)
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle(
        (margin, margin, size - margin - 1, size - margin - 1),
        radius=radius,
        fill=WHITE + (255,),
        outline=(0xD8, 0xD4, 0xCC, 255),
        width=max(1, size // 64),
    )
    return draw


def _snap(v: float, grid: float = 0.5) -> float:
    return round(v / grid) * grid


def _bar_trap(
    draw: ImageDraw.ImageDraw,
    x0: float,
    x1: float,
    y_base: float,
    y_tl: float,
    y_tr: float,
) -> None:
    pts = [
        (int(round(x0)), int(round(y_base))),
        (int(round(x1)), int(round(y_base))),
        (int(round(x1)), int(round(y_tr))),
        (int(round(x0)), int(round(y_tl))),
    ]
    draw.polygon(pts, fill=BLACK + (255,))


def _draw_shroud_hood_icon_raw(size: int) -> Image.Image:
    """Three wide black bars on white: flat head center, angled outer pair (shroud silhouette)."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    white_tile(img, size)
    draw = ImageDraw.Draw(img)

    margin = max(1, size // 16)
    tile_cy = (margin + size - margin) * 0.5
    cx_i = size // 2

    head_f = 0.36
    veil_f = 0.44
    outer_f = 0.615
    base_f = 0.935
    hem_extra_f = 0.034
    shape_cy = size * (head_f + base_f) * 0.5
    y_shift = tile_cy - shape_cy
    grid = 1.0

    y_base = _snap(size * (base_f + hem_extra_f) + y_shift, grid)
    y_head = _snap(size * head_f + y_shift, grid)
    y_veil = _snap(size * veil_f + y_shift, grid)
    y_outer = _snap(size * outer_f + y_shift, grid)
    y_outer = max(y_outer, y_veil + size * 0.055)
    outer_shorten = _snap(size * 0.028, grid)
    y_outer_bar = y_outer + outer_shorten
    y_veil_outer = y_veil + outer_shorten
    # Raise the whole outer top by the same amount so the inner corner meets the center bar.
    raise_outer = y_veil_outer - y_head
    y_outer_bar -= raise_outer
    y_veil_outer = y_head
    if size <= 48:
        nudge = int(round(((size - 1) * 0.5) - ((y_head + y_base) * 0.5)))
        y_base += nudge
        y_head += nudge
        y_outer_bar += nudge
        y_veil_outer += nudge

    span = int(round(size * 0.64))
    if span % 2:
        span -= 1
    gap_i = max(2, int(round(span * 0.14)))
    pitch_i = max(2, (span - 2 * gap_i) // 3)
    leftover = span - (3 * pitch_i + 2 * gap_i)
    gap_i += leftover // 2
    x0 = cx_i - (3 * pitch_i + 2 * gap_i) // 2

    for i in range(3):
        left = x0 + i * (pitch_i + gap_i)
        right = left + pitch_i
        if i == 0:
            _bar_trap(draw, float(left), float(right), y_base, y_outer_bar, y_veil_outer)
        elif i == 1:
            _bar_trap(draw, float(left), float(right), y_base, y_head, y_head)
        else:
            _bar_trap(draw, float(left), float(right), y_base, y_veil_outer, y_outer_bar)

    return img


def _mirror_horizontal(img: Image.Image) -> Image.Image:
    """Copy the left half onto the right so the icon is exact about the center line."""
    width, height = img.size
    pixels = img.load()
    for y in range(height):
        for x in range(width // 2):
            pixels[width - 1 - x, y] = pixels[x, y]
    return img


def draw_shroud_hood_icon(size: int) -> Image.Image:
    if size <= 48:
        img = _draw_shroud_hood_icon_raw(size)
    else:
        ref = size if size >= 1024 else size * 2
        img = _draw_shroud_hood_icon_raw(ref)
        if ref != size:
            img = img.resize((size, size), Image.Resampling.LANCZOS)
    return _mirror_horizontal(img)


def draw_icon_legacy_eye(size: int) -> Image.Image:
    """Original eye motif kept for production until a preview is chosen."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = rounded_tile(img, size)
    margin = max(1, size // 16)
    _ = margin

    cx, cy = size * 0.5, size * 0.52
    eye_w = size * 0.56
    eye_h = size * 0.30
    ring = max(2, size // 18)

    glow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    glow_draw = ImageDraw.Draw(glow)
    glow_draw.ellipse(
        (
            cx - eye_w * 0.62,
            cy - eye_h * 1.35,
            cx + eye_w * 0.62,
            cy + eye_h * 1.35,
        ),
        outline=ACID + (70,),
        width=max(1, ring),
    )
    img = Image.alpha_composite(img, glow)
    draw = ImageDraw.Draw(img)

    def eye_polygon(sw: float, sh: float):
        pts = []
        steps = 24
        for i in range(steps):
            t = i / steps * math.tau
            x = cx + math.cos(t) * sw
            y = cy + math.sin(t) * sh * (1.15 if abs(math.cos(t)) < 0.35 else 1.0)
            pts.append((x, y))
        return pts

    draw.polygon(eye_polygon(eye_w * 0.5, eye_h * 0.55), outline=TEXT + (235,), width=max(1, ring))

    iris_r = eye_h * 0.42
    draw.ellipse(
        (cx - iris_r, cy - iris_r, cx + iris_r, cy + iris_r),
        fill=AMBER + (255,),
        outline=TEXT + (200,),
        width=max(1, size // 80),
    )
    pupil_r = iris_r * 0.38
    draw.ellipse((cx - pupil_r, cy - pupil_r, cx + pupil_r, cy + pupil_r), fill=INK + (255,))
    glint_r = max(1, size // 28)
    gx = cx - iris_r * 0.35
    gy = cy - iris_r * 0.35
    draw.ellipse((gx - glint_r, gy - glint_r, gx + glint_r, gy + glint_r), fill=CYAN + (220,))
    return img


PREVIEW_VARIANTS: dict[str, tuple[str, Callable[[int], Image.Image]]] = {
    "01_horizon_slit": ("1 - Horizon slit", idea_01_horizon_image),
    "04_monogram_d": ("4 - Monogram D", lambda s: render(s, idea_04_monogram)),
    "06_pedal_stamp": ("6 - Pedal stamp", lambda s: render(s, idea_06_pedal)),
    "08_shroud_bars": ("8 - Shroud bars", lambda s: render(s, idea_08_shroud)),
}


def make_contact_sheet(keys: list[str], px: int = 512) -> Image.Image:
    cols = 2
    rows = (len(keys) + 1) // 2
    pad = 24
    label_h = 28
    cell = px + label_h
    sheet = Image.new("RGBA", (cols * cell + pad * (cols + 1), rows * cell + pad * (rows + 1)), INK + (255,))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("segoeui.ttf", 18)
    except OSError:
        font = ImageFont.load_default()

    for idx, key in enumerate(keys):
        col, row = idx % cols, idx // cols
        ox = pad + col * (cell + pad)
        oy = pad + row * (cell + pad)
        label, factory = PREVIEW_VARIANTS[key]
        icon = factory(px)
        sheet.paste(icon, (ox, oy), icon)
        draw.text((ox, oy + px + 4), label, fill=TEXT + (255,), font=font)
    return sheet


def draw_plugin_mark(size: int) -> Image.Image:
    """Title-grey shroud bars on transparency, cropped so the silhouette fills the frame."""
    src = draw_shroud_hood_icon(max(size, 512)).convert("RGBA")
    width, height = src.size
    pixels = src.load()
    coverage = Image.new("L", (width, height), 0)
    cover = coverage.load()
    min_x, min_y, max_x, max_y = width, height, 0, 0
    found = False
    for y in range(height):
        for x in range(width):
            red, _green, _blue, alpha = pixels[x, y]
            if alpha < 16:
                continue
            ink = (255 - red) * alpha // 255
            if ink <= 12:
                continue
            cover[x, y] = ink
            found = True
            min_x = min(min_x, x)
            min_y = min(min_y, y)
            max_x = max(max_x, x)
            max_y = max(max_y, y)
    if not found:
        return Image.new("RGBA", (size, size), (0, 0, 0, 0))
    crop = coverage.crop((min_x, min_y, max_x + 1, max_y + 1))
    mark = Image.new("RGBA", crop.size, (0, 0, 0, 0))
    dest = mark.load()
    shade = crop.load()
    for y in range(crop.size[1]):
        for x in range(crop.size[0]):
            ink = shade[x, y]
            if ink:
                dest[x, y] = (0xE6, 0xE2, 0xD8, ink)
    mark = _mirror_horizontal(mark)
    pad = max(1, size // 48)
    avail = size - pad * 2
    scale = min(avail / mark.size[0], avail / mark.size[1])
    fitted_w = max(1, int(round(mark.size[0] * scale)))
    fitted_h = max(1, int(round(mark.size[1] * scale)))
    fitted = mark.resize((fitted_w, fitted_h), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.paste(fitted, ((size - fitted_w) // 2, (size - fitted_h) // 2), fitted)
    return _mirror_horizontal(canvas)


def write_production() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for name, px in (("doomgazer_icon_512.png", 512), ("doomgazer_icon_32.png", 32)):
        draw_shroud_hood_icon(px).save(OUT / name, optimize=True)
        print(f"wrote {OUT / name}")
    mark_path = OUT / "doomgazer_mark.png"
    draw_plugin_mark(512).save(mark_path, optimize=True)
    print(f"wrote {mark_path}")
    hood_preview = PREVIEW_DIR / "preview_shroud_hood_512.png"
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    draw_shroud_hood_icon(512).save(hood_preview, optimize=True)
    print(f"wrote {hood_preview}")


def write_previews(keys: list[str] | None = None) -> None:
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    chosen = keys or list(PREVIEW_VARIANTS.keys())
    for key in chosen:
        if key not in PREVIEW_VARIANTS:
            raise SystemExit(f"unknown preview key: {key}")
        label, factory = PREVIEW_VARIANTS[key]
        for px in (512, 32):
            path = PREVIEW_DIR / f"preview_{key}_{px}.png"
            factory(px).save(path, optimize=True)
            print(f"wrote {path} ({label})")

    sheet = make_contact_sheet(chosen, 512)
    sheet_path = PREVIEW_DIR / "preview_sheet_1_4_6_8.png"
    sheet.save(sheet_path, optimize=True)
    print(f"wrote {sheet_path}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--previews", action="store_true", help="Write icon_previews/ only")
    parser.add_argument("--production", action="store_true", help="Write shipping icon PNGs (shroud hood)")
    parser.add_argument(
        "--pick",
        nargs="*",
        metavar="KEY",
        help="Preview keys, e.g. 01_horizon_slit 04_monogram_d",
    )
    args = parser.parse_args()
    if args.previews or args.pick is not None:
        write_previews(args.pick)
    if args.production or (not args.previews and args.pick is None):
        write_production()


if __name__ == "__main__":
    main()
