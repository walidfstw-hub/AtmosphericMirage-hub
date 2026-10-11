"""Keep the small, explicitly traced mesh set available to packaged-game CPU ray setup."""

import unreal


TRACE_ASSETS = (
    "/Game/Mirage/Imported/Kenney/CarKit/SM_Sedan",
    "/Game/Mirage/Imported/Kenney/Watercraft/SM_CargoShip",
    "/Game/Mirage/Imported/PolyHaven/Rocks/SM_NamaqualandRocks01",
)
MARKER_PATH = "/Game/Mirage/Generated/SM_MirageMarker"


if not unreal.EditorAssetLibrary.does_asset_exist(MARKER_PATH):
    marker = unreal.EditorAssetLibrary.duplicate_asset(
        "/Engine/BasicShapes/Cylinder.Cylinder", MARKER_PATH
    )
    if not marker:
        raise RuntimeError("Could not create project-local mirage marker mesh")

for path in (*TRACE_ASSETS, MARKER_PATH):
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError(f"Missing trace static mesh: {path}")
    mesh.set_editor_property("allow_cpu_access", True)
    mesh.modify()
    if not unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save CPU-access setting: {path}")
    unreal.log(f"Runtime trace CPU access enabled: {path}")

unreal.log("AtmosphericMirage: packaged trace meshes prepared")
