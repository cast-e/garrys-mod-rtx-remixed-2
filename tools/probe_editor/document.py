"""Map-specific cubemap authoring data, independent of the viewer and renderer."""
import math
import uuid
import hashlib
import json


def vector(value, name, positive=False):
    if not isinstance(value, list) or len(value) != 3:
        raise ValueError(f'{name} must contain three numbers')
    if any(isinstance(v, bool) or not isinstance(v, (float, int)) or not math.isfinite(v)
           or abs(v) > 1e8 or (positive and v <= 0) for v in value):
        raise ValueError(f'Invalid {name}')
    return list(value)


def validate(document, map_hash):
    if not isinstance(document, dict) or document.get('version') != 1 or document.get('units') != 'bsp':
        raise ValueError('Expected version 1 cubemap bounds in BSP units')
    if document.get('mapSha256') != map_hash:
        raise ValueError('These bounds belong to a different BSP revision')
    volumes = document.get('volumes')
    if not isinstance(volumes, list) or len(volumes) > 4096:
        raise ValueError('Expected at most 4096 cubemap volumes')
    result, ids = [], set()
    for volume in volumes:
        if not isinstance(volume, dict):
            raise ValueError('Invalid cubemap volume')
        identity = volume.get('id')
        if not isinstance(identity, str) or len(identity) > 80 or not identity or identity in ids:
            raise ValueError('Cubemap IDs must be unique nonempty strings')
        ids.add(identity)
        priority = volume.get('priority')
        if isinstance(priority, bool) or not isinstance(priority, int) or not -32768 <= priority <= 32767:
            raise ValueError('Priority must be an integer from -32768 to 32767')
        name = volume.get('name', '')
        if not isinstance(name, str) or len(name) > 200:
            raise ValueError('Invalid cubemap name')
        result.append(dict(id=identity, name=name, priority=priority,
                           center=vector(volume.get('center'), 'center'),
                           halfSize=vector(volume.get('halfSize'), 'halfSize', True),
                           rotationDegrees=vector(volume.get('rotationDegrees'), 'rotationDegrees'),
                           capture=vector(volume.get('capture'), 'capture')))
    clean = dict(version=1, units='bsp', mapSha256=map_hash,
                 mapName=str(document.get('mapName', ''))[:200], volumes=result)
    if 'reference' in document:
        reference = document['reference']
        if not isinstance(reference, dict) or not isinstance(reference.get('activeHash'), str) or \
                not reference['activeHash'].isdigit() or len(reference['activeHash']) > 20:
            raise ValueError('Invalid reference capture identity')
        digest = reference.get('sha256')
        if not isinstance(digest, str) or len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest):
            raise ValueError('Invalid reference report fingerprint')
        clean['reference'] = dict(activeHash=reference['activeHash'], sha256=digest)
    return clean


def empty(map_name, map_hash):
    return dict(version=1, units='bsp', mapName=map_name, mapSha256=map_hash, volumes=[])


def new_volume(position):
    return dict(id=str(uuid.uuid4()), name='Cubemap', priority=0, center=list(position),
                halfSize=[128, 128, 128], rotationDegrees=[0, 0, 0], capture=list(position))


def from_probe_report(payload, map_name, map_hash):
    """Import an explicitly selected native report; it has no BSP fingerprint.

    Native report coordinates are host units already. metersPerUnit is metadata,
    not a multiplier to apply again. Stable source IDs and report provenance
    support subsequent fit comparisons.
    """
    report = json.loads(payload)
    if not isinstance(report, dict) or report.get('version') != 1 or \
            str(report.get('activeHash', '0')) in ('', '0'):
        raise ValueError('Expected a published version 1 probe report')
    scale = report.get('metersPerUnit')
    if isinstance(scale, bool) or not isinstance(scale, (int, float)) or not math.isfinite(scale) or scale <= 0:
        raise ValueError('Invalid probe report scene scale')
    probes = report.get('probes')
    if not isinstance(probes, list) or len(probes) > 4096:
        raise ValueError('Invalid probe report size')
    result = empty(map_name, map_hash)
    result['reference'] = dict(activeHash=str(report['activeHash']), sha256=hashlib.sha256(payload).hexdigest())
    for index, probe in enumerate(probes):
        if not isinstance(probe, dict):
            raise ValueError('Invalid probe record')
        lo, hi = vector(probe.get('minimum'), 'minimum'), vector(probe.get('maximum'), 'maximum')
        if any(a >= b for a, b in zip(lo, hi)):
            raise ValueError('Invalid projection bounds in probe report')
        volume = dict(id=f'auto-{index}-{report["activeHash"]}', name=f'Automatic {index}', priority=0,
                      center=[(a+b)/2 for a,b in zip(lo,hi)], halfSize=[(b-a)/2 for a,b in zip(lo,hi)],
                      rotationDegrees=[0, 0, 0], capture=vector(probe.get('position'), 'position'))
        result['volumes'].append(volume)
    return validate(result, map_hash)
