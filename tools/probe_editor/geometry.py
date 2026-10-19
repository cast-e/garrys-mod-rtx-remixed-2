"""Read bounded world-brush preview geometry using the existing BSP inspector."""
import hashlib
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from probe_bounds import Bsp


def load(path):
    bsp = Bsp(path)
    vertices = list(struct.iter_unpack('<3f', bsp.lump(3)))
    edges = list(struct.iter_unpack('<2H', bsp.lump(12)))
    surfedges = [entry[0] for entry in struct.iter_unpack('<i', bsp.lump(13))]
    faces, models = bsp.lump(7), bsp.lump(14)
    texinfo = bsp.lump(6)
    if len(texinfo) % 72:
        raise ValueError('Unsupported BSP texture-info layout')
    if len(faces) % 56 or len(models) < 48:
        raise ValueError('Unsupported BSP world face/model layout')
    first, count = struct.unpack_from('<2i', models, 40)
    if first < 0 or count < 0 or first + count > len(faces) // 56 or count > 1000000:
        raise ValueError('Invalid world face range')
    lines, mesh, used = [], [], set()
    for face in range(first, first + count):
        texture = struct.unpack_from('<h', faces, face * 56 + 10)[0]
        if 0 <= texture < len(texinfo) // 72:
            flags = struct.unpack_from('<I', texinfo, texture * 72 + 64)[0]
            if flags & 0x86:  # SURF_SKY2D | SURF_SKY | SURF_NODRAW
                continue
        start, length = struct.unpack_from('<ih', faces, face * 56 + 4)
        if start < 0 or length < 0 or start + length > len(surfedges):
            raise ValueError('Invalid world face edges')
        polygon = []
        for signed_edge in surfedges[start:start + length]:
            edge = abs(signed_edge)
            if edge >= len(edges):
                raise ValueError('Invalid world edge')
            a, b = edges[edge]
            if max(a, b) >= len(vertices):
                raise ValueError('Invalid world vertex')
            segment = [*vertices[a], *vertices[b]]
            if not all(math.isfinite(v) and abs(v) <= 1e8 for v in segment):
                raise ValueError('Invalid world coordinates')
            polygon.append(vertices[b if signed_edge < 0 else a])
            if edge not in used:
                used.add(edge)
                lines.append(segment)
        # BSP brush faces are convex; a fan preserves their winding. Rendering is
        # double sided so the editor remains usable both inside and outside rooms.
        for i in range(1, len(polygon) - 1):
            triangle = [polygon[0], polygon[i], polygon[i + 1]]
            u, v = [[p[j] - triangle[0][j] for j in range(3)] for p in triangle[1:]]
            normal = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]]
            length = math.hypot(*normal)
            if length < 1e-8:
                continue
            normal = [n / length for n in normal]
            for point in triangle:
                mesh.extend([*point, *normal])
    return dict(name=path.stem, sha256=hashlib.sha256(bsp.data).hexdigest(), lines=lines, mesh=mesh,
                note='Solid world-brush preview; displacement base polygons only. Props and materials are not displayed. Bounds handles remain visible through walls.')
