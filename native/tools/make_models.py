#!/usr/bin/env python3
"""Generates the sample models in assets/models/.

Like the textures, they're made from code so the repository needs no
downloads and has no licensing questions. Plain Python, no packages:

    python3 tools/make_models.py

Output:
    assets/models/barrel.obj   a barrel (OBJ text, with normals and uvs)
    assets/models/rock.gltf    a lumpy rock (glTF 2.0, data embedded as base64,
                               no normals in the file: the importer computes them)

Both are in meters with counter-clockwise front faces, as the engine expects.
"""

import base64
import json
import math
import os
import random
import struct


# ---------------------------------------------------------------- barrel (OBJ)
def barrel_radius(t):
    """Radius at height fraction t (0 = bottom, 1 = top): bulges in the middle."""
    return 0.32 + 0.08 * math.sin(math.pi * t)


def make_barrel(path, segments=24, rings=8, height=1.0):
    lines = [
        "# Barrel made by tools/make_models.py",
        "# Lines the engine doesn't use (mtllib, o, usemtl, s) are kept to show they're skipped.",
        "mtllib barrel.mtl",
        "o Barrel",
    ]
    # Side: a grid of (segments + 1) x (rings + 1). The extra column repeats
    # the first one with u = 1, so the texture wraps without a seam.
    for r in range(rings + 1):
        t = r / rings
        radius = barrel_radius(t)
        for s in range(segments + 1):
            a = 2 * math.pi * s / segments
            lines.append(f"v {radius * math.sin(a):.5f} {t * height:.5f} {radius * math.cos(a):.5f}")
    for r in range(rings + 1):
        for s in range(segments + 1):
            lines.append(f"vt {s / segments:.5f} {r / rings:.5f}")
    # Side normals: outward, tilted by the bulge's slope (d radius / d height).
    for r in range(rings + 1):
        t = r / rings
        slope = 0.08 * math.pi * math.cos(math.pi * t) / height
        for s in range(segments + 1):
            a = 2 * math.pi * s / segments
            n = (math.sin(a), -slope, math.cos(a))
            length = math.sqrt(sum(c * c for c in n))
            lines.append("vn {:.5f} {:.5f} {:.5f}".format(*(c / length for c in n)))

    lines.append("usemtl wood")
    lines.append("s 1")
    cols = segments + 1
    for r in range(rings):
        for s in range(segments):
            # OBJ indices start at 1. Positions, uvs and normals share one grid
            # here, so all three indices of a corner are the same number.
            a = r * cols + s + 1
            b = a + 1
            c = a + cols
            d = c + 1
            # Seen from outside, angle (s) grows to the right: a b d c is counter-clockwise.
            lines.append(f"f {a}/{a}/{a} {b}/{b}/{b} {d}/{d}/{d} {c}/{c}/{c}")

    # Caps: a center vertex and a fan, with their own flat normals and uvs.
    side_vertices = (rings + 1) * cols
    side_uvs = side_vertices
    side_normals = side_vertices
    for cap, (t, ny) in enumerate(((1.0, 1.0), (0.0, -1.0))):
        radius = barrel_radius(t)
        y = t * height
        center = side_vertices + 1
        lines.append(f"v 0 {y:.5f} 0")
        lines.append("vt 0.5 0.5")
        lines.append(f"vn 0 {ny:.1f} 0")
        center_uv = side_uvs + 1
        normal = side_normals + 1
        ring_start = center + 1
        for s in range(segments):
            a = 2 * math.pi * s / segments
            lines.append(f"v {radius * math.sin(a):.5f} {y:.5f} {radius * math.cos(a):.5f}")
            lines.append(f"vt {0.5 + 0.5 * math.sin(a):.5f} {0.5 + 0.5 * math.cos(a):.5f}")
        for s in range(segments):
            i = ring_start + s
            j = ring_start + (s + 1) % segments
            iu = center_uv + 1 + s
            ju = center_uv + 1 + (s + 1) % segments
            if ny > 0:  # top, seen from above: center, i, j is counter-clockwise
                lines.append(f"f {center}/{center_uv}/{normal} {i}/{iu}/{normal} {j}/{ju}/{normal}")
            else:  # bottom, seen from below: reversed
                lines.append(f"f {center}/{center_uv}/{normal} {j}/{ju}/{normal} {i}/{iu}/{normal}")
        side_vertices += 1 + segments
        side_uvs += 1 + segments
        side_normals += 1

    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


# ---------------------------------------------------------------- rock (glTF)
def icosphere(subdivisions):
    t = (1 + math.sqrt(5)) / 2
    verts = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t),
             (0, -1, -t), (0, 1, -t), (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    verts = [tuple(c / math.sqrt(sum(x * x for x in v)) for c in v) for v in verts]
    faces = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2),
             (10, 7, 6), (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5),
             (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    for _ in range(subdivisions):
        cache = {}

        def midpoint(i, j):
            key = (min(i, j), max(i, j))
            if key not in cache:
                m = [(a + b) / 2 for a, b in zip(verts[i], verts[j])]
                length = math.sqrt(sum(c * c for c in m))
                verts.append(tuple(c / length for c in m))
                cache[key] = len(verts) - 1
            return cache[key]

        new_faces = []
        for a, b, c in faces:
            ab, bc, ca = midpoint(a, b), midpoint(b, c), midpoint(c, a)
            new_faces += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        faces = new_faces
    return verts, faces


def make_rock(path):
    rng = random.Random(3)
    verts, faces = icosphere(2)
    # Lumps: push each vertex in or out by smooth random bumps.
    bumps = [(tuple(rng.uniform(-1, 1) for _ in range(3)), rng.uniform(0.08, 0.2)) for _ in range(9)]
    positions = []
    uvs = []
    for v in verts:
        r = 0.5
        for direction, size in bumps:
            d = math.sqrt(sum(c * c for c in direction))
            dot = sum(a * b / d for a, b in zip(v, direction))
            r += size * max(0.0, dot) ** 4 - 0.03
        positions.append(tuple(c * r for c in v))
        # glTF's uv origin is the top-left of the image (the importer flips it).
        uvs.append((0.5 + math.atan2(v[0], v[2]) / (2 * math.pi), 0.5 - math.asin(max(-1, min(1, v[1]))) / math.pi))

    # Check the winding: the icosphere faces above are counter-clockwise from outside.
    pos_bytes = b"".join(struct.pack("<3f", *p) for p in positions)
    uv_bytes = b"".join(struct.pack("<2f", *t) for t in uvs)
    index_bytes = b"".join(struct.pack("<3H", *f) for f in faces)
    buffer = index_bytes + pos_bytes + uv_bytes  # index data is 2-byte aligned, the floats after it stay 4-byte aligned
    assert len(index_bytes) % 4 == 0

    mins = [min(p[i] for p in positions) for i in range(3)]
    maxs = [max(p[i] for p in positions) for i in range(3)]
    gltf = {
        "asset": {"version": "2.0", "generator": "xordor tools/make_models.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        # The node squashes the rock a little, so the importer's node transform is exercised.
        "nodes": [{"name": "Rock", "mesh": 0, "scale": [1.0, 0.7, 1.0]}],
        "meshes": [{"name": "Rock", "primitives": [{"attributes": {"POSITION": 1, "TEXCOORD_0": 2}, "indices": 0}]}],
        "buffers": [{"byteLength": len(buffer),
                     "uri": "data:application/octet-stream;base64," + base64.b64encode(buffer).decode("ascii")}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(index_bytes), "target": 34963},
            {"buffer": 0, "byteOffset": len(index_bytes), "byteLength": len(pos_bytes), "target": 34962},
            {"buffer": 0, "byteOffset": len(index_bytes) + len(pos_bytes), "byteLength": len(uv_bytes), "target": 34962},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5123, "count": len(faces) * 3, "type": "SCALAR"},
            {"bufferView": 1, "componentType": 5126, "count": len(positions), "type": "VEC3", "min": mins, "max": maxs},
            {"bufferView": 2, "componentType": 5126, "count": len(uvs), "type": "VEC2"},
        ],
    }
    with open(path, "w", newline="\n") as f:
        json.dump(gltf, f, indent=1)
        f.write("\n")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.normpath(os.path.join(here, "..", "assets", "models"))
    os.makedirs(out_dir, exist_ok=True)
    make_barrel(os.path.join(out_dir, "barrel.obj"))
    make_rock(os.path.join(out_dir, "rock.gltf"))
    print("wrote", out_dir)


if __name__ == "__main__":
    main()
