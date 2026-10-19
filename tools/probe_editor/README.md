# Cubemap bounds workbench

Run from `tools` with Python 3.10 or newer; no extra packages are required:

```powershell
python -m probe_editor "D:/maps/example.bsp" --output "D:/bounds/example.cubemaps.json"
```

The local browser workbench reads a Source BSP and opens top, front and side wireframe views.
Add/select volumes, drag their faces, move/rotate bounds, and move capture points independently.
Use Alt-drag on the center when the capture point overlaps it. Numeric fields expose every transform
and priority. Middle/right drag pans, wheel zooms, and Frame selected focuses the current volume.
Undo/redo, JSON import/export and Ctrl+S are available. Save atomically writes the explicit output
file; reopening the same command loads it. A BSP SHA-256 fingerprint prevents using an authoring
document against a different map revision. Close the server with Ctrl+C when finished.

To start from a published automatic fit, add `--probe-report "D:/reports/probes.json"`. Select the
report for this BSP explicitly: native probe reports currently contain no map fingerprint. Coordinates
are imported in host units without applying `metersPerUnit` again. An unchanged dashed orange overlay
shows the original fit while you edit; the Original fit checkbox hides it. Stable source IDs and the
reference report fingerprint persist in saved documents for later fitting comparisons. An existing
output file retains your edits when opened with a reference report.

Coordinates use BSP units. `center`, positive `halfSize`, and `rotationDegrees` describe an oriented
box. Rotation applies X then Y then Z (`Rz * Ry * Rx`); `capture` is independent in world space.
`id` is stable across edits. Larger integer `priority` is intended to win overlapping regions.

This first version displays world-brush edges and displacement base polygons. It does not display
props, displaced terrain tessellation or materials. It does not yet show a shaded cubemap preview
or apply the file in game. The native sampler supports rotated projection bounds and overlap priorities;
importing native reports preserves those transforms and independent capture points. Applying saved documents
through the adapter and reflection-only hot reload remain the next integration work. Do not treat
saved documents as already-active game overrides.

Checks, from the repository root:

```powershell
python tests/probe_editor.py
node tests/probe_editor_rotation.mjs
```
