"""Independent CPU validation of the HLSL curved-ray integrator.

The production path uses the same 128-step semi-implicit update as MirageRefraction.usf.
The reference path uses 65,536-step RK4 integration of d/ds(n*d)=grad(n).
"""

import math


C = 7.86e-4


def atmosphere(z, pressure, base_temp, surface_gradient, scale_height, layer_height):
    h = min(max(z, 0.0), layer_height)
    decay = math.exp(-h / scale_height)
    temperature = max(base_temp + surface_gradient * scale_height * (1.0 - decay), 180.0)
    gradient = surface_gradient * decay if 0.0 <= z <= layer_height else -0.0065
    n = 1.0 + C * pressure / temperature
    dndz = -C * pressure * gradient / (temperature * temperature)
    return n, dndz


def derivative(state, profile):
    x, z, dx, dz = state
    length = math.hypot(dx, dz)
    dx, dz = dx / length, dz / length
    n, dndz = atmosphere(z, *profile)
    projection = dz * dndz
    return dx, dz, (-dx * projection) / n, (dndz - dz * projection) / n


def shader_integrate(z, angle, distance, profile, steps=128):
    x, dx, dz = 0.0, math.cos(angle), math.sin(angle)
    step = distance / steps
    for _ in range(steps):
        n, dndz = atmosphere(z, *profile)
        projection = dz * dndz
        ddx = (-dx * projection) / n
        ddz = (dndz - dz * projection) / n
        dx, dz = dx + ddx * step, dz + ddz * step
        length = math.hypot(dx, dz)
        dx, dz = dx / length, dz / length
        x, z = x + dx * step, z + dz * step
    return x, z, math.atan2(dz, dx)


def rk4_integrate(z, angle, distance, profile, steps=65536):
    state = [0.0, z, math.cos(angle), math.sin(angle)]
    step = distance / steps
    for _ in range(steps):
        k1 = derivative(state, profile)
        k2 = derivative([state[i] + 0.5 * step * k1[i] for i in range(4)], profile)
        k3 = derivative([state[i] + 0.5 * step * k2[i] for i in range(4)], profile)
        k4 = derivative([state[i] + step * k3[i] for i in range(4)], profile)
        state = [state[i] + step * (k1[i] + 2*k2[i] + 2*k3[i] + k4[i]) / 6.0 for i in range(4)]
        length = math.hypot(state[2], state[3])
        state[2], state[3] = state[2] / length, state[3] / length
    return state[0], state[1], math.atan2(state[3], state[2])


cases = (
    ("desert_level", 1.5, 0.0, 6500.0, (101.325, 340.0, -90.0, 0.45, 8.0)),
    ("desert_down", 1.5, -0.00015, 6500.0, (101.325, 340.0, -90.0, 0.45, 8.0)),
    ("coast_level", 2.4, 0.0, 7500.0, (101.325, 268.0, 1.70, 12.0, 160.0)),
    ("coast_up", 2.4, 0.00025, 7500.0, (101.325, 268.0, 1.70, 12.0, 160.0)),
)

worst = 0.0
for name, z, angle, distance, profile in cases:
    production = shader_integrate(z, angle, distance, profile)
    reference = rk4_integrate(z, angle, distance, profile)
    production_bend = production[2] - angle
    reference_bend = reference[2] - angle
    error = abs(production_bend - reference_bend) / max(abs(reference_bend), 1e-12) * 100.0
    worst = max(worst, error)
    print(f"{name}: production={production_bend:.9e} rad reference={reference_bend:.9e} rad error={error:.3f}%")

print(f"maximum curvature error: {worst:.3f}%")
if worst > 10.0:
    raise SystemExit("Curvature validation failed: error exceeds 10%")
