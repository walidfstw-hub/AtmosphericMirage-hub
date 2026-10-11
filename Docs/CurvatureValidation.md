# Curved-Ray Numerical Validation

Validation date: 2026-10-11

The production shader integrates

`d/ds (n d) = grad(n)`

with 128 semi-implicit, normalized steps. Refractive index is evaluated as

`n(z) = 1 + 7.86e-4 P_kPa / T_K(z)`

using the same exponentially decaying surface-gradient temperature profile as `MirageRefraction.usf`. `Scripts/ValidateRayCurvature.py` independently compares it with a 65,536-step fourth-order Runge-Kutta reference.

| Case | Production bend (rad) | RK4 reference (rad) | Relative curvature error |
|---|---:|---:|---:|
| Desert, level launch | 1.610468675e-3 | 1.609443079e-3 | 0.064% |
| Desert, slightly downward | 1.760489224e-3 | 1.766262721e-3 | 0.327% |
| Cold coast, level launch | -2.807892452e-3 | -2.802352061e-3 | 0.198% |
| Cold coast, slightly upward | -3.072445415e-3 | -3.064571898e-3 | 0.257% |

Maximum measured curvature error: **0.327%**, within the STA-043 target of 10%.

The metric is relative error in total exit-angle bend, `abs(shader_bend-reference_bend)/abs(reference_bend)`, not image-space pixel error. The tested range covers pressure 101.325 kPa; base temperatures 268–340 K; surface gradients -90 to +1.70 K/m; exponential scale heights 0.45–12 m; layer depths 8–160 m; camera heights 1.5–2.4 m; launch angles -0.00015 to +0.00025 rad; and trace distances 6.5–7.5 km. These are the strong verification profiles used by the two maps, so the result validates the relevant operating endpoints rather than only a mild standard atmosphere.

This validates the numerical curvature calculation only. It does not by itself prove that inverted or stacked image branches are visually correct; rendered evidence is assessed separately.
