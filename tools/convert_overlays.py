#!/usr/bin/env python3
"""The game's see-through overlay movies (the battle ends, the golden guitar, the fret flames), from the browser port's
animated WebP (DATA/MOVIES/WEB/<name>.webp, 960x540 RGBA at 29.97 fps) to what the R36S player decodes
(src/overlay.c): one MPEG-1 program stream whose picture is the colour above the alpha.

  top half:    the colour, premultiplied by its alpha (black where nothing is)
  bottom half: the alpha as a grey picture (luma 16..235 = alpha 0..255)

One pl_mpeg decoder gives both halves of a frame in step; on the GPU one IYUV texture is drawn twice (the alpha half
darkens what is under it, the colour half is added), in software the halves are read side by side. The picture is the
4:3 middle of the 16:9 movie (the game's screens cover 640x480 with the 1280x720 stage the same way), each half
448x336 (whole macroblocks: the halves meet on a macroblock row).

  python3 tools/convert_overlays.py SRC.webp OUT.mpg [WIDTH HEIGHT]
Needs Pillow and numpy (the WebP decoder: ffmpeg 6 cannot read animated WebP) and ffmpeg."""
import subprocess, sys
import numpy as np
from PIL import Image

src, out = sys.argv[1], sys.argv[2]
W = int(sys.argv[3]) if len(sys.argv) > 3 else 448
H = int(sys.argv[4]) if len(sys.argv) > 4 else 336
assert W % 16 == 0 and H % 16 == 0, 'each half in whole macroblocks'
im = Image.open(src); n = getattr(im, 'n_frames', 1); sw, sh = im.size
cw = min(sw, round(sh * W / H)); cx = (sw - cw) // 2   # the middle at the picture's aspect
ff = subprocess.Popen(['ffmpeg', '-nostdin', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', f'{W}x{2 * H}', '-r', '30000/1001', '-i', '-',
                       '-vf', 'format=yuv420p', '-c:v', 'mpeg1video', '-b:v', '2500k', '-maxrate', '4000k', '-bufsize', '1835k', '-qmin', '2', '-qmax', '10',
                       '-bf', '0', '-g', '30', '-mbd', 'rd', '-trellis', '1', '-an', '-f', 'mpeg', out], stdin=subprocess.PIPE)
for i in range(n):
    im.seek(i); f = im.convert('RGBA').crop((cx, 0, cx + cw, sh)).resize((W, H), Image.LANCZOS)
    a = np.asarray(f, dtype=np.float32)
    al = a[:, :, 3:4] / 255.0
    col = np.clip(a[:, :, :3] * al + 0.5, 0, 255).astype(np.uint8)
    alp = np.repeat(np.asarray(f)[:, :, 3:4], 3, axis=2)
    ff.stdin.write(np.concatenate([col, alp], 0).tobytes())
ff.stdin.close()
sys.exit(ff.wait())
