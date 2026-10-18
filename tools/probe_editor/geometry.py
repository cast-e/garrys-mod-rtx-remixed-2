"""Read bounded world-brush wire geometry using the existing BSP inspector."""
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
    if len(faces) % 56 or len(models) < 48:
        raise ValueError('Unsupported BSP world face/model layout')
    first, count = struct.unpack_from('<2i', models, 40)
    if first < 0 or count < 0 or first + count > len(faces) // 56 or count > 1000000:
        raise ValueError('Invalid world face range')
    lines, used = [], set()
    for face in range(first, first + count):
        start, length = struct.unpack_from('<ih', faces, face * 56 + 4)
        if start < 0 or length < 0 or start + length > len(surfedges):
            raise ValueError('Invalid world face edges')
        for signed_edge in surfedges[start:start + length]:
            edge = abs(signed_edge)
            if edge >= len(edges):
                raise ValueError('Invalid world edge')
            if edge in used:
                continue
            used.add(edge)
            a, b = edges[edge]
            if max(a, b) >= len(vertices):
                raise ValueError('Invalid world vertex')
            segment = [*vertices[a], *vertices[b]]
            if not all(math.isfinite(v) and abs(v) <= 1e8 for v in segment):
                raise ValueError('Invalid world coordinates')
            lines.append(segment)
    return dict(name=path.stem, sha256=hashlib.sha256(bsp.data).hexdigest(), lines=lines,
                note='World-brush wireframe; displacement base polygons only. Props and materials are not displayed.')
