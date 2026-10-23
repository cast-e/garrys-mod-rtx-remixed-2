"""Authored projection references preserve source semantics and reject partial data."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
from probe_editor.bsp_reflections import entities, reference
from probe_editor.bsp_receivers import material_receivers

HASH = 'a' * 64
CUBE = b'{ "classname" "env_cubemap" "parallax" "1" "origin" "10 20 30" "boxmin" "-2 -3 -4" "boxmax" "5 6 7" "hammerid" "77" "sides" "128912 229012" }'


class CompiledReceivers(unittest.TestCase):
    def fixture(self):
        lumps = {7: bytearray(56 * 3), 6: bytearray(72), 2: bytearray(32),
                 43: b'maps/test/patched_material\0', 44: bytearray(4)}
        struct.pack_into('<h', lumps[7], 56 + 10, -1)
        return lumps

    def read(self, lumps):
        class Bsp:
            def lump(self, index):
                return lumps[index]
        return material_receivers(Bsp())

    def test_actual_compiled_indices_grouped_and_untextured_face_skipped(self):
        self.assertEqual(self.read(self.fixture()), [dict(
            material='maps/test/patched_material', compiledFaceIndices=[0, 2])])

    def test_invalid_assignment_chain_rejects_instead_of_guessing(self):
        for lump, offset, encoding, value in ((7, 10, '<h', 1), (7, 10, '<h', -2),
                (6, 68, '<i', 1), (2, 12, '<i', 1), (44, 0, '<i', -1),
                (44, 0, '<i', 999)):
            with self.subTest(lump=lump, value=value):
                lumps = self.fixture()
                struct.pack_into(encoding, lumps[lump], offset, value)
                with self.assertRaises(ValueError):
                    self.read(lumps)
        for lump in (7, 6, 2, 44):
            lumps = self.fixture()
            lumps[lump] += b'x'
            with self.subTest(layout=lump), self.assertRaises(ValueError):
                self.read(lumps)
        lumps = self.fixture()
        lumps[43] = b'unterminated'
        with self.assertRaises(ValueError):
            self.read(lumps)


class BspReflectionReference(unittest.TestCase):
    def test_preserves_projection_and_hammer_assignments(self):
        report = reference(CUBE, 'test', HASH)
        self.assertEqual(report['mapSha256'], HASH)
        self.assertEqual(report['cubemapEntityCount'], 1)
        record = report['volumes'][0]
        self.assertEqual(record['capture'], [10, 20, 30])
        self.assertEqual(record['projectionMinimumOffset'], [-2, -3, -4])
        self.assertEqual(record['projectionMaximumOffset'], [5, 6, 7])
        self.assertEqual(record['assignedHammerSides'], ['128912', '229012'])
        self.assertEqual(record['hammerId'], '77')
        self.assertTrue(record['captureInside'])
        self.assertNotIn('influenceMinimum', record)
        self.assertNotIn('priority', record)

    def test_only_explicit_parallax_qualifies(self):
        ordinary = CUBE.replace(b'"parallax" "1"', b'"parallax" "0"')
        absent = CUBE.replace(b'"parallax" "1"', b'')
        report = reference(ordinary + absent + CUBE, 'test', HASH)
        self.assertEqual(report['cubemapEntityCount'], 3)
        self.assertEqual(len(report['volumes']), 1)
        self.assertEqual(report['volumes'][0]['entityIndex'], 2)

    def test_angles_and_outside_capture_remain_reference_facts(self):
        payload = CUBE.replace(b'"boxmin" "-2 -3 -4"', b'"boxmin" "1 2 3" "angles" "0 90 0"')
        record = reference(payload, 'test', HASH)['volumes'][0]
        self.assertEqual(record['sourceAngles'], [0, 90, 0])
        self.assertFalse(record['captureInside'])

    def test_quoted_braces_comments_and_duplicate_outputs(self):
        payload = b'// ignored {\n{ "classname" "logic_relay" "OnTrigger" "a{b}" "OnTrigger" "c" }' + CUBE + b'\0'
        self.assertEqual(len(list(entities(payload))), 2)
        self.assertEqual(reference(payload, 'test', HASH)['volumes'][0]['entityIndex'], 1)

    def test_malformed_or_ambiguous_data_rejects(self):
        for payload in [CUBE[:-1], CUBE + b'garbage', CUBE + b'{', b'{ { } }',
                        CUBE.replace(b'"origin" "10 20 30"', b'"origin" "nan 20 30"'),
                        CUBE.replace(b'"boxmax" "5 6 7"', b'"boxmax" "-5 -6 -7"'),
                        CUBE.replace(b'"sides" "128912 229012"', b'"sides" "x"'),
                        CUBE.replace(b'"hammerid" "77"', b'"origin" "1 2 3"'),
                        CUBE.replace(b'"parallax" "1"', b'"parallax" "1" "parallax" "0"'),
                        CUBE.replace(b'"classname" "env_cubemap"', b'"classname" "env_cubemap" "classname" "other"')]:
            with self.subTest(payload=payload), self.assertRaises(ValueError):
                reference(payload, 'test', HASH)
        with self.assertRaises(ValueError):
            reference(CUBE, 'test', 'wrong fingerprint')
        with self.assertRaises(ValueError):
            reference(b' ' * ((16 << 20) + 1), 'test', HASH)

    def test_reference_budget_declines_instead_of_truncating(self):
        with patch('probe_editor.bsp_reflections.MAX_VOLUMES', 1):
            with self.assertRaises(ValueError):
                reference(CUBE + CUBE, 'test', HASH)

    def test_cli_uses_actual_bsp_fingerprint(self):
        data = bytearray(1036)
        data[:4] = b'VBSP'
        struct.pack_into('<i', data, 4, 20)
        struct.pack_into('<4i', data, 8, len(data), len(CUBE), 0, 0)
        data.extend(CUBE)
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'test.bsp'
            output = Path(folder) / 'reference.json'
            path.write_bytes(data)
            env = dict(os.environ, PYTHONPATH=str(TOOLS))
            subprocess.run([sys.executable, '-m', 'probe_editor.bsp_reflections', str(path),
                            '--compiled-receivers', '--output', str(output)],
                           env=env, check=True, capture_output=True)
            report = json.loads(output.read_text())
            self.assertEqual(report['mapSha256'], hashlib.sha256(data).hexdigest())
            self.assertEqual(len(report['volumes']), 1)
            self.assertEqual(report['compiledMaterialReceivers'], [])


if __name__ == '__main__':
    unittest.main()
