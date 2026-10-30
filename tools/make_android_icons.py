#!/usr/bin/env python3
"""Иконки Android из того же src/icon.ico, что и у Windows-версии.

Одна картинка на две платформы: правки иконки делаются в icon.ico, а этот
скрипт раскладывает её по mipmap-* в нужных размерах. ICO читается без
сторонних библиотек (32-битная запись = BITMAPINFOHEADER + BGRA снизу вверх),
PNG пишется через zlib.

    python3 tools/make_android_icons.py
"""
import os
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICO = os.path.join(ROOT, 'src', 'icon.ico')
RES = os.path.join(ROOT, 'android', 'app', 'src', 'main', 'res')

# Плотности Android: 48 dp — размер иконки на рабочем столе.
DENSITIES = {
    'mipmap-mdpi': 48,
    'mipmap-hdpi': 72,
    'mipmap-xhdpi': 96,
    'mipmap-xxhdpi': 144,
    'mipmap-xxxhdpi': 192,
}


def decode_png(data):
    """PNG -> (w, h, rgba-строки). Только 8 бит на канал, RGB и RGBA.

    В современных .ico картинки лежат готовыми PNG — распаковывать их проще,
    чем тащить зависимость: нужны только IDAT и пять фильтров строк.
    """
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise SystemExit('не PNG')
    pos = 8
    width = height = 0
    color_type = 0
    idat = b''
    while pos < len(data):
        length = struct.unpack_from('>I', data, pos)[0]
        tag = data[pos + 4: pos + 8]
        payload = data[pos + 8: pos + 8 + length]
        pos += 12 + length
        if tag == b'IHDR':
            width, height, depth, color_type = struct.unpack_from('>IIBB', payload, 0)
            if depth != 8 or color_type not in (2, 6):
                raise SystemExit('поддерживаются только 8-битные RGB/RGBA PNG')
        elif tag == b'IDAT':
            idat += payload
        elif tag == b'IEND':
            break
    raw = zlib.decompress(idat)
    bpp = 4 if color_type == 6 else 3
    stride = width * bpp
    rows = []
    prev = bytearray(stride)
    src = 0
    for _y in range(height):
        filt = raw[src]
        src += 1
        line = bytearray(raw[src: src + stride])
        src += stride
        if filt == 1:      # Sub
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif filt == 2:    # Up
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif filt == 3:    # Average
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif filt == 4:    # Paeth
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xFF
        prev = line
        if bpp == 4:
            rows.append(bytes(line))
        else:
            out = bytearray()
            for x in range(width):
                out += line[x * 3: x * 3 + 3] + b'\xff'
            rows.append(bytes(out))
    return width, height, rows


def read_ico_best(path):
    """Возвращает (w, h, rgba-строки) самой крупной 32-битной записи."""
    data = open(path, 'rb').read()
    _reserved, kind, count = struct.unpack_from('<HHH', data, 0)
    if kind != 1:
        raise SystemExit('это не ICO')
    best = None
    for i in range(count):
        w, h, _colors, _res, _planes, bpp, size, off = struct.unpack_from('<BBBBHHII', data, 6 + i * 16)
        w = w or 256
        h = h or 256
        if bpp != 32:
            continue
        if best is None or w * h > best[0] * best[1]:
            best = (w, h, size, off)
    if best is None:
        raise SystemExit('в ICO нет 32-битной записи')
    w, h, size, off = best
    if data[off: off + 8] == b'\x89PNG\r\n\x1a\n':
        # Современный .ico: внутри готовый PNG.
        return decode_png(data[off: off + size])
    header = struct.unpack_from('<IiiHHIIiiII', data, off)
    bi_size = header[0]
    bit_count = header[4]
    if bit_count != 32:
        raise SystemExit('ожидалась 32-битная запись')
    pixels = data[off + bi_size: off + bi_size + w * h * 4]
    rows = []
    for y in range(h):
        src = (h - 1 - y) * w * 4          # DIB хранится снизу вверх
        row = bytearray()
        for x in range(w):
            b, g, r, a = pixels[src + x * 4: src + x * 4 + 4]
            row += bytes((r, g, b, a))
        rows.append(bytes(row))
    return w, h, rows


def write_png(path, w, h, rows):
    raw = b''.join(b'\x00' + r for r in rows)

    def chunk(tag, payload):
        return (struct.pack('>I', len(payload)) + tag + payload +
                struct.pack('>I', zlib.crc32(tag + payload) & 0xffffffff))

    png = (b'\x89PNG\r\n\x1a\n' +
           chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
           chunk(b'IDAT', zlib.compress(raw, 9)) +
           chunk(b'IEND', b''))
    with open(path, 'wb') as f:
        f.write(png)


def scale_nearest(rows, src_w, src_h, dst_w):
    """Ближайший сосед: иконка — пиксельный арт, сглаживание ей вредит."""
    out = []
    for y in range(dst_w):
        sy = min(src_h - 1, y * src_h // dst_w)
        src = rows[sy]
        row = bytearray()
        for x in range(dst_w):
            sx = min(src_w - 1, x * src_w // dst_w)
            row += src[sx * 4: sx * 4 + 4]
        out.append(bytes(row))
    return out


def main():
    if not os.path.exists(ICO):
        raise SystemExit('нет %s' % ICO)
    w, h, rows = read_ico_best(ICO)
    print('источник: %s %dx%d' % (os.path.relpath(ICO, ROOT), w, h))
    for folder, size in DENSITIES.items():
        scaled = scale_nearest(rows, w, h, size)
        target_dir = os.path.join(RES, folder)
        os.makedirs(target_dir, exist_ok=True)
        target = os.path.join(target_dir, 'ic_launcher.png')
        write_png(target, size, size, scaled)
        print('  %-44s %dpx' % (os.path.relpath(target, ROOT), size))


if __name__ == '__main__':
    sys.exit(main())
