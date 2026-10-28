# Converts the 32-bit BMP frames written by `gh3 --frames` to PNG, with the standard library only.
import struct, sys, zlib, os

def bmp_to_png(src, dst):
    b = open(src, 'rb').read()
    if b[:2] != b'BM': raise SystemExit(f'{src}: not a BMP')
    off = struct.unpack_from('<I', b, 10)[0]
    w, h = struct.unpack_from('<ii', b, 18)
    bpp = struct.unpack_from('<H', b, 28)[0]
    if bpp not in (24, 32): raise SystemExit(f'{src}: {bpp} bpp not supported')
    bpx = bpp // 8
    flip = h > 0; h = abs(h); stride = (w * bpx + 3) & ~3
    rows = []
    for y in range(h):
        sy = h - 1 - y if flip else y
        row = b[off + sy * stride: off + sy * stride + stride]
        out = bytearray(b'\x00')
        for x in range(w):
            bb, g, r = row[x * bpx], row[x * bpx + 1], row[x * bpx + 2]
            out += bytes((r, g, bb))
        rows.append(bytes(out))
    raw = b''.join(rows)
    def chunk(t, d): c = t + d; return struct.pack('>I', len(d)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b'')
    open(dst, 'wb').write(png)

if __name__ == '__main__':
    for p in sys.argv[1:]:
        bmp_to_png(p, os.path.splitext(p)[0] + '.png')
    print(f'{len(sys.argv) - 1} converted')
