"""Draws the Telewhite icon: a violet circle with a white paper plane.

Writes every Telegram/Resources/art/icon* file and logo_256*.png:

    uv run --with pillow python3 docs/assets/telewhite_icon.py Telegram/Resources/art
"""
import math, sys
from PIL import Image, ImageDraw

SS = 4  # supersampling

def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(len(a)))

def gradient_circle(size, diameter):
    big = size * SS
    img = Image.new('RGBA', (big, big), (0, 0, 0, 0))
    top = (157, 125, 255)    # #9d7dff
    bottom = (88, 56, 230)   # #5838e6
    grad = Image.new('RGBA', (big, big))
    px = grad.load()
    for y in range(big):
        for x in range(big):
            t = (x * 0.35 + y * 0.65) / big
            px[x, y] = lerp(top, bottom, min(max(t, 0), 1)) + (255,)
    mask = Image.new('L', (big, big), 0)
    d = ImageDraw.Draw(mask)
    r = diameter * SS / 2
    c = big / 2
    d.ellipse((c - r, c - r, c + r, c + r), fill=255)
    img.paste(grad, (0, 0), mask)
    return img, c, r

def plane(img, c, r):
    d = ImageDraw.Draw(img)
    angle = math.radians(-32)
    scale = r * 0.68
    ox, oy = c - r * 0.05, c + r * 0.07
    def tr(p):
        x, y = p
        rx = x * math.cos(angle) - y * math.sin(angle)
        ry = x * math.sin(angle) + y * math.cos(angle)
        return (ox + rx * scale, oy + ry * scale)
    N = (1.05, 0.0)
    L = (-0.80, -0.62)
    C = (-0.38, 0.02)
    B = (-0.78, 0.34)
    K = (-0.30, 0.62)
    white = (255, 255, 255, 255)
    shade = (222, 214, 255, 255)
    keel = (190, 176, 255, 255)
    d.polygon([tr(N), tr(C), tr(K)], fill=keel)
    d.polygon([tr(N), tr(B), tr(C)], fill=shade)
    d.polygon([tr(N), tr(L), tr(C)], fill=white)

def render(size, diameter_ratio):
    img, c, r = gradient_circle(size, size * diameter_ratio)
    plane(img, c, r)
    return img.resize((size, size), Image.LANCZOS)



import io, struct, os

def bmp_entry(img):
    w, h = img.size
    px = img.load()
    data = bytearray()
    for y in range(h - 1, -1, -1):
        for x in range(w):
            r, g, b, a = px[x, y]
            data += bytes((b, g, r, a))
    row = ((w + 31) // 32) * 4
    mask = bytearray()
    for y in range(h - 1, -1, -1):
        bits = bytearray(row)
        for x in range(w):
            if px[x, y][3] == 0:
                bits[x // 8] |= 0x80 >> (x % 8)
        mask += bits
    header = struct.pack('<IiiHHIIiiII', 40, w, h * 2, 1, 32, 0, len(data) + len(mask), 0, 0, 0, 0)
    return bytes(header + data + mask)

def png_bytes(img):
    buf = io.BytesIO()
    img.save(buf, 'PNG', optimize=True)
    return buf.getvalue()

def write_ico(path, images):
    entries = []
    blobs = []
    offset = 6 + 16 * len(images)
    for img in images:
        w, h = img.size
        blob = png_bytes(img) if w >= 256 else bmp_entry(img)
        entries.append(struct.pack('<BBBBHHII', w % 256, h % 256, 0, 0, 1, 32, len(blob), offset))
        blobs.append(blob)
        offset += len(blob)
    with open(path, 'wb') as f:
        f.write(struct.pack('<HHH', 0, 1, len(images)))
        for e in entries:
            f.write(e)
        for b in blobs:
            f.write(b)

if __name__ == '__main__':
    art = sys.argv[1]
    cache = {}
    def icon(size, ratio=0.906):
        key = (size, ratio)
        if key not in cache:
            cache[key] = render(size, ratio)
        return cache[key]
    names = {
        'icon16.png': 16, 'icon16@2x.png': 32, 'icon32.png': 32, 'icon32@2x.png': 64,
        'icon48.png': 48, 'icon48@2x.png': 96, 'icon64.png': 64, 'icon64@2x.png': 128,
        'icon128.png': 128, 'icon128@2x.png': 256, 'icon256.png': 256, 'icon256@2x.png': 512,
        'icon512.png': 512, 'icon512@2x.png': 1024, 'logo_256.png': 256,
    }
    for name, size in names.items():
        icon(size).save(os.path.join(art, name), 'PNG', optimize=True)
    render(256, 1.0).save(os.path.join(art, 'logo_256_no_margin.png'), 'PNG', optimize=True)
    write_ico(os.path.join(art, 'icon256.ico'), [icon(16), icon(24), icon(32), icon(48), icon(256)])
    print('done')
