"""Force-build and verify the complete low-poly trace LOD for the coastal rock mesh."""

import unreal


MESH_PATH = "/Game/Mirage/Imported/PolyHaven/Rocks/SM_NamaqualandRocks01"
mesh = unreal.load_asset(MESH_PATH)
if not mesh:
    raise RuntimeError(f"Missing mesh: {MESH_PATH}")

subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
unreal.log(f"MIRAGE_LOD subsystem={subsystem}")
unreal.log(f"MIRAGE_LOD before={subsystem.get_lod_count(mesh)}")

base = unreal.EditorScriptingMeshReductionSettings()
base.set_editor_property("percent_triangles", 1.0)
base.set_editor_property("screen_size", 1.0)
trace = unreal.EditorScriptingMeshReductionSettings()
trace.set_editor_property("percent_triangles", 0.025)
trace.set_editor_property("screen_size", 0.01)
options = unreal.EditorScriptingMeshReductionOptions()
options.set_editor_property("auto_compute_lod_screen_size", False)
options.set_editor_property("reduction_settings", [base, trace])

result = subsystem.set_lods(mesh, options)
unreal.log(f"MIRAGE_LOD set_lods_result={result} count={subsystem.get_lod_count(mesh)}")
unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)

# Loading a fresh object after the save catches commandlet-only transient results.
unreal.SystemLibrary.collect_garbage()
reloaded = unreal.load_asset(MESH_PATH)
count = subsystem.get_lod_count(reloaded)
unreal.log(f"MIRAGE_LOD saved_count={count}")
if count < 2:
    raise RuntimeError("The reduced trace LOD did not persist")

unreal.SystemLibrary.quit_editor()
