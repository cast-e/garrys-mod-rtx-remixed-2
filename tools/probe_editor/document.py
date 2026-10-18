"""Map-specific cubemap authoring data, independent of the viewer and renderer."""
import math
import uuid


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
    return dict(version=1, units='bsp', mapSha256=map_hash,
                mapName=str(document.get('mapName', ''))[:200], volumes=result)


def empty(map_name, map_hash):
    return dict(version=1, units='bsp', mapName=map_name, mapSha256=map_hash, volumes=[])


def new_volume(position):
    return dict(id=str(uuid.uuid4()), name='Cubemap', priority=0, center=list(position),
                halfSize=[128, 128, 128], rotationDegrees=[0, 0, 0], capture=list(position))
