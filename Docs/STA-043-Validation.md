# STA-043 implementation and verification report

Date: 2026-10-11  
Engine: Unreal Engine 5.6.1  
Project: `AtmosphericMirage`

## Overall result

**PARTIAL — not every requirement can be claimed as passed.** The physically curved renderer, complete compact trace geometry, both mirage demonstrations, performance tests, and Windows package are implemented and visually verified. The remaining material limitation is exact evaluation of hidden/off-screen Unreal material graphs: visible triangle hits reproject the real rendered surface, while unavailable surface samples fall back to interpolated normals and a class base colour. The inferior silver pool is physically produced and visible, but its planar verification profile still reads as a fairly regular horizontal band rather than a fully photoreal turbulent road mirage.

The referenced “existing task specification” and “approved-assets addendum” were not present in the project or supplied attachment. This table therefore covers the requirements in the supplied closeout request and prior STA-043 instructions; it is verification status, not client acceptance.

## Requirement matrix

| Requirement | Status | Evidence / result |
|---|---|---|
| Scene-view extension active and shader composited | PASS | Gameplay and packaged logs contain construction, Tonemap subscription, and `Dispatching MirageRefractionPS`; diagnostic mode is saved as 0. |
| Physical refractive-index equation and curved integration | PASS | `n(z)=1+7.86e-4 P_kPa/T_K(z)`, explicit cm-to-m conversion, 128 normalized semi-implicit steps. Independent RK4 maximum error 0.327%. |
| Geometry-aware visibility, not SceneColor UV warp alone | PASS | Tagged objects use ray/triangle intersections. SceneDepth is only a no-explicit-geometry fallback. Desert traces 2,026 sedan + 512 marker triangles; coast traces 1,058 ship + 2,142 complete reduced-cliff triangles. |
| No silent geometry truncation or accepted box proxy | PASS | Full selected LOD counts are uploaded. The 4,096-triangle compact limit contains all 3,200 coastal triangles. Packaged logs report identical complete counts. |
| Cliff solution | PASS | Normal-editor LOD generation produced a persistent two-LOD rock/cliff asset; the complete 2,142-triangle reduced LOD preserves the photographed rock silhouette. No engine source was changed. |
| Normals/UV/material fidelity | PARTIAL | Barycentric normals and UV0 are uploaded. Visible hit points are projected to the existing Tonemap input and sample the mesh’s real shaded/texture colour without a second tonemap. Hidden/off-screen samples use interpolated-normal lighting plus class base colour; arbitrary Unreal material graphs are not evaluated in HLSL. |
| Desert upright sedan + separate inverted image | PASS (verification) | Final ON crop shows the light-blue sedan above and its separate vertically reversed blue silhouette below; OFF contains only the upright car. The distant branch is optically compressed, as expected for the strong controlled profile. |
| Inverted marker | PASS (verification) | The red marker points upward in the direct image and downward in the separate lower branch; it is absent from OFF. |
| Sky-derived silver road pool | PARTIAL | The pool exists only in ON, is resolved by sky-returning curved branches, transmits through the asphalt, responds to camera height/profile, and is confined by the world-space heated-road volume. Its boundary remains more regular than a photoreal road mirage. |
| Patch moves farther as camera height rises | PASS | Identical 20° FOV/exposure comparisons at 0.5 m and 3.0 m show the near edge moving toward the road vanishing point. Revised apparent-distance estimate: about 72 m versus 367 m, or about 295 m farther; uncertainty is documented below. |
| ColdCoast ship looming/stretching/stacking | PASS (verification) | ON crop visibly contains multiple displaced hull and superstructure bands from the actual 1,058-triangle cargo ship; OFF has one ship. |
| ColdCoast cliff inversion/stacking | PASS (verification) | ON crop contains multiple separated texture-bearing rock/headland bands from the actual complete reduced mesh; OFF has one rock silhouette. |
| Runtime controls/settings display | PASS | Effect, height presets, gradient, pressure, map switch, and help controls are implemented; active values are displayed. See `Docs/Controls.md`. |
| Editor/game build, HLSL, tests, maps/assets | PASS | Editor and game targets build; cook-time shaders compiled; automation test passed; both maps and required assets passed commandlet validation. |
| Performance measurement | PASS | At 1423×889 on the recorded hardware: Desert 129.85 FPS / 7.70 ms frame / 6.76 ms GPU; Coast 46.75 FPS / 21.39 ms frame / 20.36 ms GPU. See `Docs/Performance.md`. |
| Windows package and launch both maps | PASS | Final Development package built/cooked/staged. Direct packaged runs loaded both maps, dispatched the shader, reported complete triangle counts, captured frames, and exited cleanly. |
| Short demonstration video | NOT CAPTURED | No reliable video-recording interface was available. Full-resolution ON/OFF stills and enlarged crops are supplied instead. |

## Rendering implementation

- The module maps `/AtmosphericMirage` to the project shader directory and constructs the view extension during startup.
- `MirageRefraction.usf` evaluates the exponential temperature profile and integrates `d/ds(n d)=grad(n)` through a world-space atmospheric volume.
- The renderer resolves primary and neighboring angular branches, enabling inferior inversion and superior upright/inverted stacking.
- Triangle positions, inverse-transpose transformed normals, and UV0 are extracted from the exact world transforms. Möller–Trumbore segment/triangle tests return barycentrics.
- The refracting footprint is a physical world-space volume (road/sea width and X range), not a stencil, duplicated object, post-process material, or painted overlay.
- `Allow CPU Access` is enabled only for the compact traced assets and project-local marker so packaged builds can construct the triangle buffers. A defensive runtime guard reports missing cooked data instead of asserting.

## DesertRoad visual evidence

Final matched pairs (native window capture 1423×889; requested 1920×1080 was constrained by the 1707×1067 desktop work area):

- 0.5 m OFF: `Saved/Screenshots/MirageVerification/DesertRoad_Submission2_H050_OFF.png`
- 0.5 m ON: `Saved/Screenshots/MirageVerification/DesertRoad_Submission2_H050_ON.png`
- 3.0 m OFF: `Saved/Screenshots/MirageVerification/DesertRoad_Submission2_H300_OFF.png`
- 3.0 m ON: `Saved/Screenshots/MirageVerification/DesertRoad_Submission2_H300_ON.png`
- Enlarged crops: `Saved/Screenshots/MirageVerification/Crops/DesertRoad_Submission2_*_crop.png`

All pairs use the same X/Y position, pitch, 20° horizontal FOV, fixed exposure, and diagnostic mode 0. The paired OFF frame is an exact SceneColor passthrough through the already-warmed callback, avoiding first-frame render-target differences.

### Camera-height measurement

The earlier 107.1 m / 575.0 m values came from an older 1280×720 capture before the camera-rotation and continuous-road corrections and are not retained as final measurements.

For the final 1423-pixel-wide, 20° horizontal-FOV captures, horizontal focal length is `f=(1423/2)/tan(10°)=4035 px`. Using each image’s road vanishing row and the visually inspected near edge of the silver branch gives:

| Height | Vanishing row | Near edge row | Apparent ground distance |
|---:|---:|---:|---:|
| 0.5 m | 436 | 464 | `0.5*4035/(464-436) = 72 m` |
| 3.0 m | 443 | 476 | `3.0*4035/(476-443) = 367 m` |

The apparent displacement is about **295 m farther** at 3.0 m. Selecting the antialiased edge by ±2 pixels gives roughly 67–78 m and 347–403 m respectively, so displacement uncertainty is approximately ±35 m. These are pinhole ground-plane appearance estimates, not direct physical ray/road intersection distances. The numerical integrator’s return-height sweep is separately recorded by `Scripts/AnalyzeInferiorMapping.py`.

## ColdCoast visual evidence

- OFF: `Saved/Screenshots/MirageVerification/ColdCoast_FinalEvidence_H150_OFF.png`
- ON: `Saved/Screenshots/MirageVerification/ColdCoast_FinalEvidence_H150_ON.png`
- Ship crops: `Saved/Screenshots/MirageVerification/Crops/ColdCoast_FinalEvidence_H150_*_ship_crop.png`
- Cliff crops: `Saved/Screenshots/MirageVerification/Crops/ColdCoast_FinalEvidence_H150_*_cliff_crop.png`

Visual inspection shows geometry-derived stretching and several displaced ship bands, plus separated upright/inverted rock bands. Their silhouettes and surface colours correspond to the actual meshes; they are not compound boxes or duplicated level actors.

## Packaged evidence

- Package: `Saved/StagedBuilds/STA043-Windows/Windows/AtmosphericMirage.exe`
- Archived directory size: 1,041,892,187 bytes (0.970 GiB, 68 files)
- Launcher SHA-256: `07D62FB318DB3A7D4F49CB35B5A38AF51E7A3A66B0FE49D0C6387CEE26F93DAF`
- Desert frame: `Saved/StagedBuilds/STA043-Windows/Windows/AtmosphericMirage/Saved/Screenshots/MirageVerification/Packaged_Final_DesertRoad.png`
- Coast frame: `Saved/StagedBuilds/STA043-Windows/Windows/AtmosphericMirage/Saved/Screenshots/MirageVerification/Packaged_Final_ColdCoast.png`
- Runtime logs: `Saved/Logs/Packaged_Final_DesertRoad.log`, `Saved/Logs/Packaged_Final_ColdCoast.log`

## Verification results

- `AtmosphericMirageEditor Win64 Development`: succeeded.
- `AtmosphericMirage Win64 Development`: succeeded.
- Unreal automation `AtmosphericMirage.Controller.Defaults`: Success; report at `Saved/Automation/STA043/index.html`.
- Level/reference commandlet: Success, 0 errors (one benign EyeAdaptation cvar-priority warning).
- Cook/package: Success; both maps explicitly listed in packaging settings.
- Curvature reference: 0.327% maximum relative bend-angle error.

## Submission contents and known limitations

Deliver the project directory and/or the staged Windows directory; no upload was performed. Third-party provenance remains in `Docs/AssetSourceLog.md` and source archives/manifests under `SourceAssets`.

Known limitations are the hidden/off-screen material fallback, the deliberately strong/regular verification profiles, and the lack of a demonstration video. These prevent a claim that every presentation-quality criterion is fully passed, but they do not revert to SceneColor-only warping, AABB proxy acceptance, or fake duplicated images.
