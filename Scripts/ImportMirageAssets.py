"""Import the approved CC0 assets used by the two mirage demonstration maps.

Run with UnrealEditor-Cmd.exe AtmosphericMirage.uproject -run=pythonscript
-script=Scripts/ImportMirageAssets.py. Source files remain in SourceAssets for auditability.
"""

from pathlib import Path
import unreal


PROJECT = Path(unreal.Paths.project_dir())
SOURCE = PROJECT / "SourceAssets" / "Extracted"
DEST_ROOT = "/Game/Mirage/Imported"
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def import_fbx(source, destination, name, import_materials=False):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)

    options = unreal.FbxImportUI()
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("import_materials", import_materials)
    options.set_editor_property("import_textures", import_materials)
    options.static_mesh_import_data.set_editor_property("combine_meshes", True)
    options.static_mesh_import_data.set_editor_property("generate_lightmap_u_vs", True)
    options.static_mesh_import_data.set_editor_property("auto_generate_collision", True)
    task.set_editor_property("options", options)
    asset_tools.import_asset_tasks([task])
    if not task.imported_object_paths:
        raise RuntimeError(f"FBX import produced no assets: {source}")
    unreal.log(f"Imported {source.name} -> {task.imported_object_paths}")
    return task.imported_object_paths[0]


def import_texture(source, destination, name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    asset_tools.import_asset_tasks([task])
    if not task.imported_object_paths:
        raise RuntimeError(f"Texture import produced no assets: {source}")
    return unreal.load_asset(task.imported_object_paths[0])


def configure_texture(texture, normal=False, linear=False):
    if normal:
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("srgb", False)
    elif linear:
        texture.set_editor_property("srgb", False)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)


def make_pbr_material(name, destination, color, normal, roughness, ao=None, tiling=1.0):
    path = f"{destination}/{name}"
    existing = unreal.load_asset(path)
    if existing:
        unreal.EditorAssetLibrary.delete_asset(path)
    material = asset_tools.create_asset(name, destination, unreal.Material, unreal.MaterialFactoryNew())
    uv = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -850, 80
    )
    uv.set_editor_property("u_tiling", tiling)
    uv.set_editor_property("v_tiling", tiling)

    def sample(texture, x, y, sampler_type):
        node = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionTextureSample, x, y
        )
        node.set_editor_property("texture", texture)
        node.set_editor_property("sampler_type", sampler_type)
        unreal.MaterialEditingLibrary.connect_material_expressions(uv, "", node, "UVs")
        return node

    base = sample(color, -560, -130, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    norm = sample(normal, -560, 30, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rough = sample(roughness, -560, 190, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(norm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    unreal.MaterialEditingLibrary.connect_material_property(rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    if ao:
        ao_node = sample(ao, -560, 350, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        unreal.MaterialEditingLibrary.connect_material_property(
            ao_node, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION
        )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def main():
    car = SOURCE / "KenneyCarKit" / "Models" / "FBX format"
    boats = SOURCE / "KenneyWatercraftKit" / "Models" / "FBX format"
    poly = SOURCE / "PolyHaven"
    asphalt = SOURCE / "AmbientCG" / "Asphalt027C"

    for path in (car, boats, poly, asphalt):
        if not path.exists():
            raise RuntimeError(f"Required approved source directory is missing: {path}")

    import_fbx(car / "sedan.fbx", f"{DEST_ROOT}/Kenney/CarKit", "SM_Sedan", True)
    import_fbx(car / "truck.fbx", f"{DEST_ROOT}/Kenney/CarKit", "SM_Truck", True)
    import_fbx(car / "garbage-truck.fbx", f"{DEST_ROOT}/Kenney/CarKit", "SM_GarbageTruck", True)
    import_fbx(boats / "ship-cargo-a.fbx", f"{DEST_ROOT}/Kenney/Watercraft", "SM_CargoShip", True)
    import_fbx(boats / "ship-ocean-liner.fbx", f"{DEST_ROOT}/Kenney/Watercraft", "SM_OceanLiner", True)
    import_fbx(boats / "buoy.fbx", f"{DEST_ROOT}/Kenney/Watercraft", "SM_Buoy", True)

    rock_dir = poly / "namaqualand_rocks_01"
    cliff_dir = poly / "namaqualand_cliff_02"
    import_fbx(rock_dir / "namaqualand_rocks_01_2k.fbx", f"{DEST_ROOT}/PolyHaven/Rocks", "SM_NamaqualandRocks01")
    import_fbx(cliff_dir / "namaqualand_cliff_02_2k.fbx", f"{DEST_ROOT}/PolyHaven/Cliffs", "SM_NamaqualandCliff02")

    rock_tex_dest = f"{DEST_ROOT}/PolyHaven/Rocks/Textures"
    rock_color = import_texture(rock_dir / "textures" / "namaqualand_rocks_01_diff_2k.jpg", rock_tex_dest, "T_Rocks_D")
    rock_normal = import_texture(rock_dir / "textures" / "namaqualand_rocks_01_nor_gl_2k.exr", rock_tex_dest, "T_Rocks_N")
    rock_rough = import_texture(rock_dir / "textures" / "namaqualand_rocks_01_rough_2k.exr", rock_tex_dest, "T_Rocks_R")
    configure_texture(rock_normal, normal=True)
    configure_texture(rock_rough, linear=True)
    make_pbr_material("M_NamaqualandRocks", f"{DEST_ROOT}/PolyHaven/Rocks", rock_color, rock_normal, rock_rough)

    cliff_tex_dest = f"{DEST_ROOT}/PolyHaven/Cliffs/Textures"
    cliff_color = import_texture(cliff_dir / "textures" / "namaqualand_cliff_02_diff_2k.jpg", cliff_tex_dest, "T_Cliff_D")
    cliff_normal = import_texture(cliff_dir / "textures" / "namaqualand_cliff_02_nor_gl_2k.exr", cliff_tex_dest, "T_Cliff_N")
    cliff_rough = import_texture(cliff_dir / "textures" / "namaqualand_cliff_02_rough_2k.exr", cliff_tex_dest, "T_Cliff_R")
    configure_texture(cliff_normal, normal=True)
    configure_texture(cliff_rough, linear=True)
    make_pbr_material("M_NamaqualandCliff", f"{DEST_ROOT}/PolyHaven/Cliffs", cliff_color, cliff_normal, cliff_rough)

    asphalt_dest = f"{DEST_ROOT}/AmbientCG/Asphalt027C"
    asphalt_color = import_texture(asphalt / "Asphalt027C_2K-JPG_Color.jpg", asphalt_dest, "T_Asphalt_D")
    asphalt_normal = import_texture(asphalt / "Asphalt027C_2K-JPG_NormalDX.jpg", asphalt_dest, "T_Asphalt_N")
    asphalt_rough = import_texture(asphalt / "Asphalt027C_2K-JPG_Roughness.jpg", asphalt_dest, "T_Asphalt_R")
    asphalt_ao = import_texture(asphalt / "Asphalt027C_2K-JPG_AmbientOcclusion.jpg", asphalt_dest, "T_Asphalt_AO")
    configure_texture(asphalt_normal, normal=True)
    configure_texture(asphalt_rough, linear=True)
    configure_texture(asphalt_ao, linear=True)
    make_pbr_material(
        "M_Asphalt027C", asphalt_dest, asphalt_color, asphalt_normal, asphalt_rough, asphalt_ao, tiling=80.0
    )

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log("AtmosphericMirage: approved CC0 asset import complete")


main()
