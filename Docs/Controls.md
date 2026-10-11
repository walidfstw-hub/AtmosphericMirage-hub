# Runtime controls and verification views

## Controls

| Key | Action |
|---|---|
| `M` | Toggle mirage rendering ON/OFF |
| `1` | Set camera height to 0.5 m above the active road/sea layer |
| `2` | Set camera height to 1.5 m |
| `3` | Set camera height to 3.0 m |
| `[` / `]` | Decrease / increase surface temperature gradient |
| `-` / `=` | Decrease / increase pressure |
| `F5` | Load DesertRoad |
| `F6` | Load ColdCoast |
| `H` | Toggle the on-screen controls help |

The HUD displays effect state, camera height, `dT/dz`, and pressure. Renderer settings are uploaded only when a value changes; geometry is not rebuilt every tick.

## Saved verification cameras

All coordinates are Unreal centimetres.

| Map | Presets | Position / orientation | Evidence FOV |
|---|---|---|---:|
| DesertRoad | 0.5, 1.5, 3.0 m | X=-4500, Y=0, Z=50/150/300; pitch=-0.18°, yaw=0°, roll=0° | 20° |
| ColdCoast | 0.5, 1.5, 3.0 m above sea Z=-5 | X=-4200, Y=0, Z=45/145/295; pitch=0.05°, yaw=0°, roll=0° | 40° |

Normal interactive cameras retain the map’s 62° FOV. The narrower evidence FOV is supplied by `-MirageFOV` and does not move scene objects.

## Launch

Editor project: open `AtmosphericMirage.uproject` in UE 5.6.1 and play `DesertRoad` or `ColdCoast`.

Packaged build: run `Saved/StagedBuilds/STA043-Windows/Windows/AtmosphericMirage.exe`. DesertRoad is the default map; use `F6` for ColdCoast.
