# Asset Source Log

All third-party assets below were checked before download and are licensed under Creative Commons Zero (CC0 1.0). Attribution is not legally required, but author/source credits are retained here for provenance. Download date: 2026-10-11.

| Asset | Author | Source | Exact license | Attribution | Unreal destination |
|---|---|---|---|---|---|
| Car Kit 3.1 (`sedan`, `truck`, `garbage-truck`) | Kenney | https://kenney.nl/assets/car-kit | CC0 1.0 Universal | Not required; credit to Kenney appreciated | `/Game/Mirage/Imported/Kenney/CarKit` |
| Watercraft Kit 2.1 (`ship-cargo-a`, `ship-ocean-liner`, `buoy`) | Kenney | https://kenney.nl/assets/watercraft-kit | CC0 1.0 Universal | Not required; credit to Kenney appreciated | `/Game/Mirage/Imported/Kenney/Watercraft` |
| Namaqualand Rocks 01 | Greg Zaal (photography), Jenelle van Heerden (modeling) | https://polyhaven.com/a/namaqualand_rocks_01 | CC0 1.0 Universal | Not required | `/Game/Mirage/Imported/PolyHaven/Rocks` |
| Namaqualand Cliff 02 | Dario Barresi (photography), Rico Cilliers (modeling) | https://polyhaven.com/a/namaqualand_cliff_02 | CC0 1.0 Universal | Not required | `/Game/Mirage/Imported/PolyHaven/Cliffs` |
| Asphalt 027 C, 2K JPG PBR set | ambientCG | https://ambientcg.com/view?id=Asphalt027C | CC0 1.0 Universal | Not required | `/Game/Mirage/Imported/AmbientCG/Asphalt027C` |

## Local source audit trail

- Original archives and Poly Haven API manifests are retained under `SourceAssets/Downloads`.
- Extracted source FBX and texture files are retained under `SourceAssets/Extracted`.
- The exact license texts supplied by Kenney are retained in each extracted pack.
- `Scripts/ImportMirageAssets.py` performs the reproducible Unreal import and fails if a required source or import output is absent.
- `/Game/Mirage/Generated/SM_MirageMarker` is a project-local copy of Unreal's built-in cylinder primitive created solely to retain cooked CPU triangle data. It is not an externally downloaded asset and is not counted among the licensed third-party imports above.
- The traced car, cargo ship, rock/cliff LOD, and marker have `Allow CPU Access` enabled by `Scripts/PrepareRuntimeTraceMeshes.py`; this is required for packaged runtime triangle-buffer construction.
