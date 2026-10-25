"""Software-render the native asset-test's JSON (development QA only)."""
import argparse
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image


def matrix(joint):
    _, x, y, z, rx, ry, rz, sx, sy, sz = joint
    cx, cy, cz = map(math.cos, (rx, ry, rz))
    ax, ay, az = map(math.sin, (rx, ry, rz))
    r = np.array([[cy*cz, ax*ay*cz-cx*az, cx*ay*cz+ax*az, x],
                  [cy*az, ax*ay*az+cx*cz, cx*ay*az-ax*cz, y],
                  [-ay, ax*cy, cx*cy, z], [0, 0, 0, 1]], dtype=float)
    return r @ np.diag([sx, sy, sz, 1])


def render(source, output):
    data = json.loads(source.read_text())
    joints = []
    for j in data["joints"]:
        local = matrix(j)
        joints.append(local if j[0] < 0 else joints[j[0]] @ local)
    canvas = np.full((640, 640, 3), (38, 43, 50), dtype=np.uint8)
    depth = np.full((640, 640), -float("inf"))
    angle = math.radians(-28)
    view = np.array([[math.cos(angle), 0, math.sin(angle)],
                     [0, 1, 0], [-math.sin(angle), 0, math.cos(angle)]])
    for tid, *vertices in data["triangles"]:
        xyz = np.array([(view @ (joints[v[0]] @ np.array([*v[1:4], 1]))[:3])
                        for v in vertices])
        xy = xyz[:, :2] * [1.1, -1.1] + [320, 560]
        x0, y0 = np.maximum(xy.min(axis=0).astype(int), 0)
        x1, y1 = np.minimum(np.ceil(xy.max(axis=0)).astype(int), 639)
        if x1 < x0 or y1 < y0:
            continue
        a, b, c = xy
        denominator = (b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(denominator) < 0.001:
            continue
        xx, yy = np.meshgrid(np.arange(x0, x1+1)+0.5, np.arange(y0, y1+1)+0.5)
        u = ((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1]))/denominator
        v = ((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1]))/denominator
        w = 1-u-v
        z = u*xyz[0, 2]+v*xyz[1, 2]+w*xyz[2, 2]
        tw, th, pixels, mode_s, mode_t = data["textures"][tid]
        uv = np.array([v[4:6] for v in vertices])
        tx = np.floor(u*uv[0, 0]+v*uv[1, 0]+w*uv[2, 0]).astype(int)
        ty = np.floor(u*uv[0, 1]+v*uv[1, 1]+w*uv[2, 1]).astype(int)
        def address(coord, size, mode):
            if mode & 2:
                return np.clip(coord, 0, size-1)
            if mode & 1:
                coord = coord % (size*2)
                return np.where(coord >= size, size*2-1-coord, coord)
            return coord % size
        tx = address(tx, tw, mode_s)
        ty = address(ty, th, mode_t)
        tex = np.array(pixels, dtype=np.uint16).reshape(th, tw)[ty, tx]
        rgb = np.stack([(tex & 31)*255//31, ((tex>>5)&31)*255//31,
                        ((tex>>10)&31)*255//31], axis=2).astype(np.uint8)
        region = depth[y0:y1+1, x0:x1+1]
        mask = (u >= 0) & (v >= 0) & (w >= 0) & (z > region) & (tex != 0)
        region[mask] = z[mask]
        canvas[y0:y1+1, x0:x1+1][mask] = rgb[mask]
    Image.fromarray(canvas).save(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    render(args.source, args.output)
