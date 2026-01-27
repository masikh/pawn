import math
import os
import struct
import sys
import zlib


def _clamp01(x: float) -> float:
    if x < 0.0:
        return 0.0
    if x > 1.0:
        return 1.0
    return x


def _smoothstep(edge0: float, edge1: float, x: float) -> float:
    if edge0 == edge1:
        return 0.0
    t = _clamp01((x - edge0) / (edge1 - edge0))
    return t * t * (3.0 - 2.0 * t)


def _lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def _circle_sdf(x: float, y: float, cx: float, cy: float, r: float) -> float:
    dx = x - cx
    dy = y - cy
    return math.sqrt(dx * dx + dy * dy) - r


def _ellipse_sdf(x: float, y: float, cx: float, cy: float, rx: float, ry: float) -> float:
    dx = x - cx
    dy = y - cy
    nx = dx / rx
    ny = dy / ry
    return (math.sqrt(nx * nx + ny * ny) - 1.0) * min(rx, ry)


def _rounded_rect_sdf(x: float, y: float, hx: float, hy: float, r: float) -> float:
    ax = abs(x)
    ay = abs(y)
    qx = ax - (hx - r)
    qy = ay - (hy - r)
    mx = max(qx, 0.0)
    my = max(qy, 0.0)
    outside = math.sqrt(mx * mx + my * my)
    inside = min(max(qx, qy), 0.0)
    return outside + inside - r


def _hash12(x: float, y: float) -> float:
    return math.fmod(math.sin(x * 127.1 + y * 311.7) * 43758.5453123, 1.0)


def _noise2(x: float, y: float) -> float:
    ix = math.floor(x)
    iy = math.floor(y)
    fx = x - ix
    fy = y - iy
    u = fx * fx * (3.0 - 2.0 * fx)
    v = fy * fy * (3.0 - 2.0 * fy)
    a = _hash12(ix + 0.0, iy + 0.0)
    b = _hash12(ix + 1.0, iy + 0.0)
    c = _hash12(ix + 0.0, iy + 1.0)
    d = _hash12(ix + 1.0, iy + 1.0)
    ab = a + (b - a) * u
    cd = c + (d - c) * u
    return ab + (cd - ab) * v


def _fbm2(x: float, y: float) -> float:
    v = 0.0
    a = 0.55
    for _ in range(5):
        v += a * _noise2(x, y)
        x = x * 2.02 + 13.37
        y = y * 2.02 + 7.91
        a *= 0.5
    return v


def _png_chunk(chunk_type: bytes, data: bytes) -> bytes:
    length = struct.pack(">I", len(data))
    crc = zlib.crc32(chunk_type + data) & 0xFFFFFFFF
    return length + chunk_type + data + struct.pack(">I", crc)


def _png_from_rgba(width: int, height: int, rgba: bytes) -> bytes:
    stride = width * 4
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw.extend(rgba[y * stride : (y + 1) * stride])

    compressed = zlib.compress(bytes(raw), level=9)
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)

    return (
        b"\x89PNG\r\n\x1a\n"
        + _png_chunk(b"IHDR", ihdr)
        + _png_chunk(b"IDAT", compressed)
        + _png_chunk(b"IEND", b"")
    )


def _render_icon_rgba(size: int) -> bytes:
    rgba = bytearray(size * size * 4)

    edge = 1.25 / float(size)

    bg_top = (8.0, 9.0, 10.0)
    bg_bottom = (0.0, 0.0, 0.0)

    for y in range(size):
        fy = y / float(size - 1)

        for x in range(size):
            fx = x / float(size - 1)

            px = fx - 0.5
            py = fy - 0.5

            rx = px
            ry = py
            rot = -0.40
            cr = math.cos(rot)
            sr = math.sin(rot)
            tx = rx * cr - ry * sr
            ty = rx * sr + ry * cr
            tx *= 1.02
            ty *= 1.02

            bg_d = _rounded_rect_sdf(px, py, 0.5, 0.5, 0.12)
            bg_alpha = 1.0 - _smoothstep(-edge, edge, bg_d)

            bg_r = _lerp(bg_top[0], bg_bottom[0], fy)
            bg_g = _lerp(bg_top[1], bg_bottom[1], fy)
            bg_b = _lerp(bg_top[2], bg_bottom[2], fy)

            smoke1 = _fbm2(px * 2.4 + 2.3, py * 2.4 + 0.9)
            smoke2 = _fbm2(px * 5.2 - 4.1, py * 5.2 + 3.7)
            smoke = _clamp01(smoke1 * 0.65 + smoke2 * 0.35)
            smoke = (smoke - 0.35) * 1.35
            smoke = _clamp01(smoke)
            bg_r = _lerp(bg_r, 28.0, smoke * 0.25)
            bg_g = _lerp(bg_g, 28.0, smoke * 0.25)
            bg_b = _lerp(bg_b, 28.0, smoke * 0.25)

            d_head = _circle_sdf(tx, ty, 0.0, -0.23, 0.13)
            d_neck = _ellipse_sdf(tx, ty, 0.0, -0.05, 0.10, 0.12)
            d_body = _ellipse_sdf(tx, ty, 0.0, 0.11, 0.18, 0.24)
            d_base1 = _ellipse_sdf(tx, ty, 0.0, 0.32, 0.28, 0.10)
            d_base2 = _ellipse_sdf(tx, ty, 0.0, 0.40, 0.30, 0.07)
            pawn_d = min(d_head, d_neck, d_body, d_base1, d_base2)

            pawn_alpha = 1.0 - _smoothstep(-edge, edge, pawn_d)

            shadow_d = min(
                _circle_sdf(tx - 0.015, ty - 0.018, 0.0, -0.23, 0.13),
                _ellipse_sdf(tx - 0.015, ty - 0.018, 0.0, -0.05, 0.10, 0.12),
                _ellipse_sdf(tx - 0.015, ty - 0.018, 0.0, 0.11, 0.18, 0.24),
                _ellipse_sdf(tx - 0.015, ty - 0.018, 0.0, 0.32, 0.28, 0.10),
                _ellipse_sdf(tx - 0.015, ty - 0.018, 0.0, 0.40, 0.30, 0.07),
            )
            shadow_alpha = (1.0 - _smoothstep(-edge, edge, shadow_d)) * 0.18

            marble = _fbm2(tx * 7.5 + 1.2, ty * 7.5 - 0.4)
            marble = _clamp01(marble)
            marble = marble * marble
            base_r = _lerp(9.0, 24.0, marble)
            base_g = _lerp(10.0, 25.0, marble)
            base_b = _lerp(12.0, 30.0, marble)

            v = _fbm2(tx * 12.0 + 10.0, ty * 12.0 - 6.0)
            v = abs(v - 0.5)
            vein = 1.0 - _smoothstep(0.03, 0.11, v)
            vein *= _smoothstep(-0.02, 0.12, -pawn_d)

            light = _clamp01(0.42 + 0.85 * (-tx * 0.65 - ty * 0.35))
            rim = 1.0 - _smoothstep(0.0, 0.02, abs(pawn_d))
            spec = _clamp01((light - 0.72) * 3.0) * (0.45 + 0.55 * rim)

            pawn_r = base_r
            pawn_g = base_g
            pawn_b = base_b

            pawn_r = _lerp(pawn_r, 214.0, vein * 0.35)
            pawn_g = _lerp(pawn_g, 170.0, vein * 0.32)
            pawn_b = _lerp(pawn_b, 82.0, vein * 0.22)

            pawn_r = _lerp(pawn_r, 255.0, spec * 0.35)
            pawn_g = _lerp(pawn_g, 255.0, spec * 0.35)
            pawn_b = _lerp(pawn_b, 255.0, spec * 0.35)

            base_disk = _ellipse_sdf(tx, ty, 0.06, 0.42, 0.20, 0.075)
            base_disk_alpha = 1.0 - _smoothstep(-edge, edge, base_disk)
            base_disk_alpha *= pawn_alpha
            teal = _clamp01(0.85 + 0.35 * _fbm2(tx * 8.0 + 3.0, ty * 8.0 + 2.0))
            teal_r = _lerp(0.0, 18.0, teal)
            teal_g = _lerp(120.0, 210.0, teal)
            teal_b = _lerp(130.0, 235.0, teal)

            out_r = bg_r
            out_g = bg_g
            out_b = bg_b

            out_r = _lerp(out_r, 0.0, shadow_alpha)
            out_g = _lerp(out_g, 0.0, shadow_alpha)
            out_b = _lerp(out_b, 0.0, shadow_alpha)

            out_r = _lerp(out_r, pawn_r, pawn_alpha)
            out_g = _lerp(out_g, pawn_g, pawn_alpha)
            out_b = _lerp(out_b, pawn_b, pawn_alpha)

            out_r = _lerp(out_r, teal_r, base_disk_alpha)
            out_g = _lerp(out_g, teal_g, base_disk_alpha)
            out_b = _lerp(out_b, teal_b, base_disk_alpha)

            out_a = bg_alpha

            idx = (y * size + x) * 4
            rgba[idx + 0] = int(_clamp01(out_r / 255.0) * 255.0)
            rgba[idx + 1] = int(_clamp01(out_g / 255.0) * 255.0)
            rgba[idx + 2] = int(_clamp01(out_b / 255.0) * 255.0)
            rgba[idx + 3] = int(_clamp01(out_a) * 255.0)

    return bytes(rgba)


def _write_icns(path: str) -> None:
    entries = []

    sizes = [
        (16, b"icp4"),
        (32, b"icp5"),
        (64, b"icp6"),
        (128, b"ic07"),
        (256, b"ic08"),
        (512, b"ic09"),
        (1024, b"ic10"),
    ]

    for size, icon_type in sizes:
        rgba = _render_icon_rgba(size)
        png = _png_from_rgba(size, size, rgba)
        entries.append((icon_type, png))

    body = bytearray()
    for icon_type, data in entries:
        element_size = 8 + len(data)
        body.extend(icon_type)
        body.extend(struct.pack(">I", element_size))
        body.extend(data)

    total_size = 8 + len(body)

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(b"icns")
        f.write(struct.pack(">I", total_size))
        f.write(body)


def main() -> int:
    if len(sys.argv) != 2:
        return 2

    output_path = sys.argv[1]
    _write_icns(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
