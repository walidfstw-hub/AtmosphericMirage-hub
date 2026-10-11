"""Print imported static-mesh bounds for deterministic level placement."""

import unreal


for path in (
    "/Game/Mirage/Imported/Kenney/CarKit/SM_Sedan",
    "/Game/Mirage/Imported/Kenney/CarKit/SM_Truck",
    "/Game/Mirage/Imported/Kenney/CarKit/SM_GarbageTruck",
    "/Game/Mirage/Imported/Kenney/Watercraft/SM_CargoShip",
    "/Game/Mirage/Imported/Kenney/Watercraft/SM_OceanLiner",
    "/Game/Mirage/Imported/Kenney/Watercraft/SM_Buoy",
    "/Game/Mirage/Imported/PolyHaven/Rocks/SM_NamaqualandRocks01",
    "/Game/Mirage/Imported/PolyHaven/Cliffs/SM_NamaqualandCliff02",
):
    mesh = unreal.load_asset(path)
    if not mesh:
        raise RuntimeError(f"Missing imported asset {path}")
    bounds = mesh.get_bounds()
    unreal.log(f"MIRAGE_BOUNDS {path} origin={bounds.origin} extent={bounds.box_extent}")
