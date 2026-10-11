# STA-043 performance results

Date: 2026-10-11

## Test system and method

- OS: Windows 11 25H2 (10.0.26200.9457)
- CPU: Intel Core 7 240H, 10 physical / 16 logical cores reported by Unreal
- GPU: NVIDIA GeForce RTX 5060 Laptop GPU, 7,899 MB dedicated memory
- RHI: D3D12, Shader Model 6
- Resolution: 1423×889 windowed (desktop work area constrained the requested 1920×1080 window)
- Build: UE 5.6.1 Development Editor game mode
- Exposure: fixed; audio disabled; diagnostic colouring disabled
- Method: opt-in `-MiragePerf` sampler, 5 s warm-up followed by about 32 s. Frame time uses game tick elapsed time; GPU time uses `RHIGetGPUFrameCycles`. Values include the full scene and renderer, not an isolated shader microbenchmark.

| Map / trace set | Frames | Average FPS | Average frame | Average GPU |
|---|---:|---:|---:|---:|
| DesertRoad: 2,026-triangle sedan + 512-triangle marker | 4,155 | 129.85 | 7.701 ms | 6.755 ms |
| ColdCoast: 1,058-triangle ship + 2,142-triangle cliff | 1,496 | 46.75 | 21.389 ms | 20.355 ms |

The compact complete-LOD approach meets the current demonstration scope without a BVH. ColdCoast is GPU-bound and is the limiting case; adding more traced high-density objects should use a BVH or additional complete reduced LODs rather than raising the flat triangle loop indiscriminately.
