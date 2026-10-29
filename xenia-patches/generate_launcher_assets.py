"""Original geometric fret icons; no third-party artwork or dependencies."""
import math
import struct
import zlib
from pathlib import Path


def write_png(path, width, height):
    colors = [(77, 192, 133), (228, 88, 89), (240, 202, 86),
              (88, 155, 222), (235, 141, 65)]
    rows = []
    unit = min(width, height)
    for y in range(height):
        row = bytearray()
        for x in range(width):
            nx = (x - width / 2) / unit
            ny = (y - height / 2) / unit
            color = (10, 19, 24)
            # A five-fret neck inside a circle, drawn from simple geometry.
            radius = math.hypot(nx, ny)
            if 0.355 < radius < 0.37:
                color = (72, 102, 106)
            if abs(nx) < 0.115 and abs(ny) < 0.285:
                color = (28, 45, 49)
            for i, fret in enumerate(colors):
                if abs(nx) < 0.145 and abs(ny - (-0.22 + i * 0.11)) < 0.027:
                    color = fret
            row.extend((*color, 255))
        rows.append(b'\0' + row)

    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack(
            '>I', zlib.crc32(kind + payload))

    data = b'\x89PNG\r\n\x1a\n'
    data += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
    data += chunk(b'IDAT', zlib.compress(b''.join(rows))) + chunk(b'IEND', b'')
    path.write_bytes(data)


def generate(folder):
    folder.mkdir(parents=True, exist_ok=True)
    for name, width, height in [('StoreLogo.png', 50, 50),
                                ('Square44x44Logo.png', 44, 44),
                                ('Square150x150Logo.png', 150, 150),
                                ('SplashScreen.png', 620, 300)]:
        write_png(folder / name, width, height)


if __name__ == '__main__':
    import sys
    generate(Path(sys.argv[1]))
