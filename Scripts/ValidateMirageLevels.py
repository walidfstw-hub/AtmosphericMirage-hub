"""Headless smoke tests for the generated demonstration maps."""

import unreal


editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def validate_level(path, expected_type, required_labels):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        raise AssertionError(f"Missing map asset: {path}")
    if not levels.load_level(path):
        raise AssertionError(f"Could not load map: {path}")

    actors = editor.get_all_level_actors()
    labels = {actor.get_actor_label() for actor in actors}
    missing = set(required_labels) - labels
    if missing:
        raise AssertionError(f"{path} is missing actors: {sorted(missing)}")

    controllers = [actor for actor in actors if actor.get_class().get_name() == "MirageController"]
    if len(controllers) != 1:
        raise AssertionError(f"{path} expected one MirageController, found {len(controllers)}")
    actual_type = controllers[0].get_editor_property("mirage_type")
    if actual_type != expected_type:
        raise AssertionError(f"{path} has {actual_type}, expected {expected_type}")
    if not controllers[0].get_editor_property("enabled"):
        raise AssertionError(f"{path} mirage renderer is disabled")
    controller = controllers[0]
    if controller.get_editor_property("pressure_k_pa") <= 0.0:
        raise AssertionError(f"{path} has invalid atmospheric pressure")
    if controller.get_editor_property("base_temperature_k") < 180.0:
        raise AssertionError(f"{path} has invalid base temperature")
    if controller.get_editor_property("trace_distance_meters") < 6000.0:
        raise AssertionError(f"{path} ray integration distance is too short")
    if controller.get_editor_property("diagnostic_mode") != 0:
        raise AssertionError(f"{path} was saved with diagnostic coloring enabled")

    static_meshes = [actor for actor in actors if isinstance(actor, unreal.StaticMeshActor)]
    if len(static_meshes) < 5:
        raise AssertionError(f"{path} has insufficient scene geometry: {len(static_meshes)} meshes")

    unreal.log(f"Validated {path}: {len(actors)} actors, {len(static_meshes)} static meshes")


for imported_asset in (
    "/Game/Mirage/Imported/Kenney/CarKit/SM_Sedan",
    "/Game/Mirage/Imported/Kenney/CarKit/SM_Truck",
    "/Game/Mirage/Imported/Kenney/CarKit/SM_GarbageTruck",
    "/Game/Mirage/Imported/Kenney/Watercraft/SM_CargoShip",
    "/Game/Mirage/Imported/Kenney/Watercraft/SM_OceanLiner",
    "/Game/Mirage/Imported/Kenney/Watercraft/SM_Buoy",
    "/Game/Mirage/Imported/PolyHaven/Rocks/SM_NamaqualandRocks01",
    "/Game/Mirage/Imported/PolyHaven/Cliffs/SM_NamaqualandCliff02",
    "/Game/Mirage/Imported/AmbientCG/Asphalt027C/M_Asphalt027C",
):
    if not unreal.EditorAssetLibrary.does_asset_exist(imported_asset):
        raise AssertionError(f"Missing imported asset reference: {imported_asset}")


validate_level(
    "/Game/Maps/DesertRoad",
    unreal.MirageType.INFERIOR,
    ["DesertFloor", "AsphaltRoad", "MirageVerificationCar", "MirageVerificationMarker",
     "DesertRock_00", "RoadPost_8000_-650", "CameraPreset_0_5m", "CameraPreset_1_5m",
     "CameraPreset_3_0m", "MiragePreviewCamera", "PlayerStart_MirageView", "MirageController_INFERIOR"],
)
validate_level(
    "/Game/Maps/ColdCoast",
    unreal.MirageType.SUPERIOR,
    ["ColdSea", "NearShore", "LighthouseTower", "DistantShipHull", "HorizonShipEast",
     "NavigationBuoy", "DistantCliff_00", "CameraPreset_0_5m", "CameraPreset_1_5m",
     "CameraPreset_3_0m", "MiragePreviewCamera", "MirageController_SUPERIOR"],
)
unreal.log("AtmosphericMirage: all level smoke tests passed")
