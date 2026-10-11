"""Build the two demonstration maps for AtmosphericMirage.

Run with UnrealEditor-Cmd.exe AtmosphericMirage.uproject -run=pythonscript
-script=Scripts/CreateMirageLevels.py. The script is idempotent: it replaces the generated maps.
"""

import unreal


LEVELS = "/Game/Maps"
MATERIALS = "/Game/Mirage/Materials"
IMPORTED = "/Game/Mirage/Imported"
GENERATED = "/Game/Mirage/Generated"
editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assets = unreal.AssetToolsHelpers.get_asset_tools()


def make_material(name, color, roughness=0.65, metallic=0.0):
    path = f"{MATERIALS}/{name}"
    existing = unreal.load_asset(path)
    if existing:
        return existing

    material = assets.create_asset(name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    color_node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -320, 0
    )
    color_node.set_editor_property("constant", unreal.LinearColor(*color, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(
        color_node, "", unreal.MaterialProperty.MP_BASE_COLOR
    )

    rough_node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -320, 120
    )
    rough_node.set_editor_property("r", roughness)
    unreal.MaterialEditingLibrary.connect_material_property(
        rough_node, "", unreal.MaterialProperty.MP_ROUGHNESS
    )

    metal_node = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -320, 220
    )
    metal_node.set_editor_property("r", metallic)
    unreal.MaterialEditingLibrary.connect_material_property(
        metal_node, "", unreal.MaterialProperty.MP_METALLIC
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def spawn_mesh(label, mesh_path, location, scale, material=None, rotation=(0, 0, 0), mirage_tags=()):
    actor = editor.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(*location),
        unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]),
    )
    actor.set_actor_label(label)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    component = actor.static_mesh_component
    component.set_static_mesh(unreal.load_asset(mesh_path))
    if material:
        component.set_material(0, material)
    component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    if mirage_tags:
        actor.set_editor_property("tags", [unreal.Name("MirageTrace")] + [unreal.Name(tag) for tag in mirage_tags])
    return actor


def required_asset(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError(f"Required imported asset is missing: {path}. Run ImportMirageAssets.py first.")
    return asset


def ensure_cliff_trace_lod(mesh_path):
    """Return the mesh whose persisted trace LOD is built by ForceCliffLOD.py.

    StaticMeshEditorSubsystem is unavailable in PythonScriptCommandlet, where the
    deprecated reduction API returns -1 without building an LOD.  Keeping LOD
    creation in the normal-editor helper makes map regeneration deterministic.
    """
    return required_asset(mesh_path)


def spawn_imported(label, mesh_path, location, scale=(1, 1, 1), material=None,
                   pitch=0.0, yaw=0.0, roll=0.0, mirage_tags=()):
    actor = editor.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location), unreal.Rotator())
    actor.set_actor_label(label)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_actor_rotation(unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll), False)
    actor.static_mesh_component.set_static_mesh(required_asset(mesh_path))
    actor.static_mesh_component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    actor.static_mesh_component.set_editor_property("cast_shadow", True)
    if material:
        actor.static_mesh_component.set_material(0, material)
    if mirage_tags:
        actor.set_editor_property("tags", [unreal.Name("MirageTrace")] + [unreal.Name(tag) for tag in mirage_tags])
    return actor


def add_environment(sun_rotation, sun_intensity, fog_density, fog_color, skylight_intensity=0.8):
    sun = editor.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(),
        unreal.Rotator(pitch=sun_rotation[0], yaw=sun_rotation[1], roll=sun_rotation[2])
    )
    sun.set_actor_label("Sun")
    sun.light_component.set_editor_property("intensity", sun_intensity)
    sun.light_component.set_editor_property("light_color", unreal.Color(255, 240, 214))
    sun.light_component.set_editor_property("atmosphere_sun_light", True)

    sky = editor.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(), unreal.Rotator())
    sky.set_actor_label("SkyAtmosphere")
    skylight = editor.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 500), unreal.Rotator())
    skylight.set_actor_label("SkyLight")
    skylight.light_component.set_editor_property("intensity", skylight_intensity)

    fog = editor.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(), unreal.Rotator())
    fog.set_actor_label("AtmosphericHaze")
    fog_component = fog.component
    fog_component.set_editor_property("fog_density", fog_density)
    fog_component.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(*fog_color, 1.0))


def add_camera(location, rotation, label="MiragePreviewCamera", player_start=False):
    camera = editor.spawn_actor_from_class(
        unreal.CameraActor, unreal.Vector(*location),
        unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2])
    )
    camera.set_actor_label(label)
    camera.camera_component.set_editor_property("field_of_view", 62.0)
    if player_start:
        player = editor.spawn_actor_from_class(
            unreal.PlayerStart, unreal.Vector(*location),
            unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2])
        )
        player.set_actor_label("PlayerStart_MirageView")
    return camera


def add_camera_presets(x, y, surface_z, pitch):
    heights = (("0_5m", 50.0), ("1_5m", 150.0), ("3_0m", 300.0))
    for suffix, height_cm in heights:
        add_camera(
            (x, y, surface_z + height_cm),
            (pitch, 0, 0),
            label=f"CameraPreset_{suffix}",
            player_start=suffix == "1_5m",
        )


def add_vehicle(label, location, materials, scale=1.0, color="truck"):
    """Build a readable vehicle silhouette instead of a placeholder block."""
    x, y, z = location
    spawn_mesh(label, "/Engine/BasicShapes/Cube.Cube", (x, y, z + 115 * scale),
               (6.5 * scale, 1.55 * scale, 0.65 * scale), materials[color])
    spawn_mesh(f"{label}_Cab", "/Engine/BasicShapes/Cube.Cube", (x - 430 * scale, y, z + 210 * scale),
               (1.9 * scale, 1.5 * scale, 1.35 * scale), materials[color])
    spawn_mesh(f"{label}_Windshield", "/Engine/BasicShapes/Cube.Cube", (x - 535 * scale, y, z + 245 * scale),
               (0.08 * scale, 1.2 * scale, 0.62 * scale), materials["glass"])
    for axle_x in (-430, 390):
        for side_y in (-175, 175):
            spawn_mesh(f"{label}_Wheel_{axle_x}_{side_y}", "/Engine/BasicShapes/Cylinder.Cylinder",
                       (x + axle_x * scale, y + side_y * scale, z + 55 * scale),
                       (0.7 * scale, 0.7 * scale, 0.42 * scale), materials["rubber"], (90, 0, 0))


def add_ship(label, location, materials, scale=1.0):
    """Create a high-contrast ship with hull, bridge and masts visible at the horizon."""
    x, y, z = location
    spawn_mesh(label, "/Engine/BasicShapes/Cube.Cube", (x, y, z),
               (55 * scale, 7 * scale, 2.2 * scale), materials["ship"])
    spawn_mesh(f"{label}_Bow", "/Engine/BasicShapes/Cone.Cone", (x + 3000 * scale, y, z + 40 * scale),
               (5.0 * scale, 5.0 * scale, 2.8 * scale), materials["ship"], (0, 90, 0))
    spawn_mesh(f"{label}_Bridge", "/Engine/BasicShapes/Cube.Cube", (x - 900 * scale, y, z + 430 * scale),
               (8 * scale, 5.5 * scale, 4.2 * scale), materials["snow"])
    for mast_x in (-300, 1150):
        spawn_mesh(f"{label}_Mast_{mast_x}", "/Engine/BasicShapes/Cylinder.Cylinder",
                   (x + mast_x * scale, y, z + 1100 * scale),
                   (0.28 * scale, 0.28 * scale, 11 * scale), materials["stripe"])


def add_controller(mirage_type, **properties):
    cls = unreal.load_class(None, "/Script/AtmosphericMirage.MirageController")
    controller = editor.spawn_actor_from_class(cls, unreal.Vector(), unreal.Rotator())
    controller.set_actor_label(f"MirageController_{mirage_type.name}")
    controller.set_editor_property("mirage_type", mirage_type)
    for key, value in properties.items():
        controller.set_editor_property(key, value)
    return controller


def begin_level(asset_path):
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)
    if not levels.new_level(asset_path):
        raise RuntimeError(f"Could not create level {asset_path}")


def save_level():
    if not levels.save_current_level():
        raise RuntimeError("Could not save generated level")


def build_desert_road(materials):
    begin_level(f"{LEVELS}/DesertRoad")
    add_environment((-24, -28, 0), 8.0, 0.0025, (0.60, 0.38, 0.18))
    # The road begins behind the player and runs continuously for 6.3 km.
    spawn_mesh("DesertFloor", "/Engine/BasicShapes/Cube.Cube", (295000, 0, -125), (6600, 1200, 1.2), materials["sand"])
    spawn_mesh("AsphaltRoad", "/Engine/BasicShapes/Cube.Cube", (295000, 0, -12), (6600, 10.8, 0.12), materials["asphalt_real"])
    for x in range(-20000, 620000, 12000):
        spawn_mesh(f"RoadStripe_{x}", "/Engine/BasicShapes/Cube.Cube", (x, 0, 2), (24, 0.075, 0.018), materials["stripe"])
    for x in range(-20000, 620000, 6000):
        for y in (-515, 515):
            spawn_mesh(f"RoadEdge_{x}_{y}", "/Engine/BasicShapes/Cube.Cube", (x, y, 1), (30, 0.035, 0.015), materials["edge"])

    rock_mesh = f"{IMPORTED}/PolyHaven/Rocks/SM_NamaqualandRocks01"
    rock_material = materials["rock_real"]
    for index, (x, y, scale, yaw) in enumerate([
        (12000, -1800, 7.0, 10), (22000, 2300, 10.0, 120), (42000, -3700, 13.0, 38),
        (70000, 4200, 18.0, -30), (105000, -6500, 25.0, 70), (155000, 8200, 32.0, 160),
        (235000, -11500, 46.0, 15), (330000, 14500, 58.0, 95), (470000, -20000, 75.0, -25),
    ]):
        spawn_imported(f"DesertRock_{index:02d}", rock_mesh, (x, y, 0), (scale, scale, scale), rock_material, yaw=yaw)

    # Controlled proof pair: at 1.8 km the independently validated -90 K/m profile
    # returns its inverted branch at 8.35..12.60 m.  The actual mesh silhouettes
    # fit that full window, preserving a readable upright/inverted separation.
    verification_car = spawn_imported(
        "MirageVerificationCar", f"{IMPORTED}/Kenney/CarKit/SM_Sedan", (180000, -200, 835),
        (3.0, 5.0, 3.2), materials["verify_car"], yaw=0,
        mirage_tags=("MirageTriangleTrace", "MirageVehicle"))
    verification_car.static_mesh_component.set_editor_property("cast_shadow", False)
    verification_marker = spawn_mesh(
        "MirageVerificationMarker", f"{GENERATED}/SM_MirageMarker", (180000, 450, 1047),
        (0.6, 0.6, 4.2), materials["marker"],
        mirage_tags=("MirageTriangleTrace", "MirageMarker"))
    verification_marker.static_mesh_component.set_editor_property("cast_shadow", False)
    for x in range(8000, 180000, 8000):
        for y in (-650, 650):
            spawn_mesh(f"RoadPost_{x}_{y}", "/Engine/BasicShapes/Cylinder.Cylinder",
                       (x, y, 60), (0.10, 0.10, 1.2), materials["post"])
    add_camera((-4500, 0, 150), (-0.18, 0, 0), player_start=False)
    add_camera_presets(-4500, 0, 0, -0.18)
    add_controller(
        unreal.MirageType.INFERIOR,
        refraction_strength=0.0018,
        horizon=0.505,
        band_height=0.28,
        turbulence_speed=1.65,
        turbulence_scale=46.0,
        reflection_amount=0.72,
        pressure_k_pa=101.325,
        base_temperature_k=340.0,
        temperature_gradient_k_per_meter=-90.0,
        temperature_scale_height_meters=0.45,
        layer_base_world_z=0.0,
        layer_thickness_meters=8.0,
        layer_lateral_half_width_meters=5.4,
        layer_start_world_x_meters=-100.0,
        layer_end_world_x_meters=6500.0,
        trace_distance_meters=6500.0,
        branch_separation_radians=0.0022,
        diagnostic_mode=0,
    )
    save_level()


def build_cold_coast(materials):
    begin_level(f"{LEVELS}/ColdCoast")
    add_environment((-14, 22, 0), 14.0, 0.0022, (0.42, 0.58, 0.72), skylight_intensity=2.0)
    spawn_mesh("ColdSea", "/Engine/BasicShapes/Cube.Cube", (330000, 0, -105), (7200, 1700, 1.0), materials["water"])
    spawn_mesh("NearShore", "/Engine/BasicShapes/Cube.Cube", (-24000, 0, -55), (220, 1700, 0.5), materials["snow"])
    spawn_mesh("LighthouseTower", "/Engine/BasicShapes/Cylinder.Cylinder", (105000, 26000, 1900), (5.0, 5.0, 38.0), materials["lighthouse"])
    spawn_mesh("LighthouseTop", "/Engine/BasicShapes/Sphere.Sphere", (105000, 26000, 4050), (6.5, 6.5, 3.2), materials["stripe"])
    # Real ship meshes are enlarged to full vessel dimensions and remain readable at the horizon.
    spawn_imported("DistantShipHull", f"{IMPORTED}/Kenney/Watercraft/SM_CargoShip", (52000, -9000, -35), (15, 15, 15), materials["ship"], mirage_tags=("MirageTriangleTrace", "MirageShip"))
    spawn_imported("HorizonShipEast", f"{IMPORTED}/Kenney/Watercraft/SM_OceanLiner", (88000, 5000, -40), (13, 13, 13), materials["liner"])
    spawn_imported("NavigationBuoy", f"{IMPORTED}/Kenney/Watercraft/SM_Buoy", (38000, 4300, -45), (4, 4, 4), materials["lighthouse"])

    # The source cliff scan contains a rectangular capture skirt, so the clean high-density
    # rock scan is scaled into overlapping coastal headlands instead of exposing that skirt.
    cliff_mesh = f"{IMPORTED}/PolyHaven/Rocks/SM_NamaqualandRocks01"
    ensure_cliff_trace_lod(cliff_mesh)
    for index, (x, y, z, sx, sy, sz, yaw) in enumerate([
        (125000, 40000, 75, 100, 250, 100, 90),
        (155000, -30000, -700, 360, 680, 680, 88),
        (185000, 16000, -600, 330, 650, 610, 94),
        (220000, 62000, -800, 390, 710, 740, 86),
    ]):
        cliff_tags = ("MirageTriangleTrace", "MirageCliff") if index == 0 else ()
        spawn_imported(f"DistantCliff_{index:02d}", cliff_mesh, (x, y, z), (sx, sy, sz), materials["rock_real"], yaw=yaw, mirage_tags=cliff_tags)
    add_camera((-4200, 0, 240), (0.05, 0, 0), player_start=False)
    add_camera_presets(-4200, 0, -5, 0.05)
    add_controller(
        unreal.MirageType.SUPERIOR,
        refraction_strength=0.0008,
        horizon=0.50,
        band_height=0.18,
        turbulence_speed=0.42,
        turbulence_scale=30.0,
        looming_amount=0.72,
        pressure_k_pa=101.325,
        base_temperature_k=268.0,
        temperature_gradient_k_per_meter=1.70,
        temperature_scale_height_meters=12.0,
        layer_base_world_z=-90.0,
        layer_thickness_meters=160.0,
        layer_lateral_half_width_meters=1700.0,
        layer_start_world_x_meters=-100.0,
        layer_end_world_x_meters=7200.0,
        trace_distance_meters=7500.0,
        branch_separation_radians=0.0120,
        diagnostic_mode=0,
    )
    save_level()


def main():
    unreal.EditorAssetLibrary.make_directory(LEVELS)
    unreal.EditorAssetLibrary.make_directory(MATERIALS)
    materials = {
        "sand": make_material("M_Sand", (0.55, 0.22, 0.055), 0.92),
        "asphalt": make_material("M_Asphalt", (0.025, 0.028, 0.032), 0.78),
        "asphalt_real": required_asset(f"{IMPORTED}/AmbientCG/Asphalt027C/M_Asphalt027C"),
        "stripe": make_material("M_Stripe", (0.95, 0.72, 0.12), 0.55),
        "edge": make_material("M_RoadEdge", (0.92, 0.91, 0.84), 0.48),
        "rock": make_material("M_Rock", (0.24, 0.075, 0.03), 0.9),
        "rock_real": required_asset(f"{IMPORTED}/PolyHaven/Rocks/M_NamaqualandRocks"),
        "cliff_real": required_asset(f"{IMPORTED}/PolyHaven/Cliffs/M_NamaqualandCliff"),
        "truck": make_material("M_Truck", (0.52, 0.035, 0.018), 0.45, 0.15),
        "car": make_material("M_Car", (0.02, 0.18, 0.52), 0.38, 0.2),
        "verify_car": make_material("M_VerificationCarBlue", (0.025, 0.12, 0.80), 0.38, 0.10),
        "utility": make_material("M_UtilityTruck", (0.80, 0.46, 0.04), 0.46, 0.12),
        "glass": make_material("M_Glass", (0.015, 0.08, 0.12), 0.12, 0.65),
        "rubber": make_material("M_Rubber", (0.008, 0.008, 0.009), 0.96),
        "post": make_material("M_RoadPost", (0.75, 0.74, 0.68), 0.65, 0.15),
        "marker": make_material("M_VerificationMarker", (0.86, 0.025, 0.01), 0.34, 0.1),
        "water": make_material("M_ColdWater", (0.018, 0.12, 0.21), 0.18, 0.35),
        "snow": make_material("M_Snow", (0.72, 0.82, 0.9), 0.82),
        "ice": make_material("M_Ice", (0.28, 0.58, 0.72), 0.34),
        "lighthouse": make_material("M_Lighthouse", (0.64, 0.04, 0.025), 0.52),
        "ship": make_material("M_Ship", (0.035, 0.065, 0.09), 0.5, 0.4),
        "liner": make_material("M_OceanLiner", (0.68, 0.71, 0.74), 0.42, 0.18),
    }
    build_desert_road(materials)
    build_cold_coast(materials)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log("AtmosphericMirage: created DesertRoad and ColdCoast")


main()
