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

The workbench displays world-brush edges and displacement base polygons. Props, displaced terrain
and materials are not displayed, and there is no shaded preview inside the browser.

For the configured testing install, launch from the repository root:

```powershell
./tools/launch_cubemap_editor.ps1 gm_construct
```

An absolute BSP path also works. Add `-ProbeReport <path>` to start from an explicitly selected
native probe report. Authoring JSON stays in `_work/cubemaps`; a compact runtime sidecar is published
to the testing install's `garrysmod/data/openremix/cubemaps/<map>.cubemaps.bin`. Live save is enabled
when a runtime output is configured. Edits publish after a 500 ms pause; Ctrl+S still saves explicitly.
The game polls twice per second and verifies the sidecar's SHA-256 against its loaded BSP.
A different BSP revision or malformed edit preserves the last valid request. Removing the sidecar
restores automatic cubemaps; saving an empty document explicitly disables local cubemaps.

For another install, invoke `python -m probe_editor` with an explicit `--runtime-output <path>`.
The adapter's `OPENREMIX_PROXY_CUBEMAP_BOUNDS` environment variable can select a diagnostic sidecar
instead of the default map-specific path. The JSON itself is not parsed by the game.

The current runtime supports at most 32 authored volumes, and capture points must remain inside
their projection bounds. Rotation and signed overlap priority are supported. Bounds/priority changes
reuse filtered radiance without new rays. Moving or adding a capture traces only the affected cubemap;
scene acceleration is reused during the authoring session. Completed lightmaps and diffuse probes
stay active throughout. The previous reflection set stays visible until a complete replacement is ready.
The native API contains generic volumes and scene IDs; BSP fingerprints and units belong to the adapter.


Checks, from the repository root:

```powershell
python tests/probe_editor.py
node tests/probe_editor_rotation.mjs
```
