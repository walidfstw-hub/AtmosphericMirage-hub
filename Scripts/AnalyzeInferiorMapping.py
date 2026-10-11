"""Find physically valid negative launch angles for the DesertRoad proof target."""

import math

C = 7.86e-4
P = 101.325
T0 = 340.0
G0 = -90.0
H = 0.45
LAYER = 8.0


def atmosphere(z):
    h = min(max(z, 0.0), LAYER)
    decay = math.exp(-h / H)
    temp = T0 + G0 * H * (1.0 - decay)
    gradient = G0 * decay if 0.0 <= z <= LAYER else -0.0065
    n = 1.0 + C * P / temp
    return n, -C * P * gradient / (temp * temp)


def trace(angle, target_x, start_z=0.5, step=6500.0 / 128.0):
    x, z = 0.0, start_z
    dx, dz = math.cos(angle), math.sin(angle)
    minimum = z
    while x < target_x:
        ds = min(step, (target_x - x) / max(dx, 1e-6))
        n, dndz = atmosphere(z)
        projection = dz * dndz
        dx += (-dx * projection / n) * ds
        dz += ((dndz - dz * projection) / n) * ds
        length = math.hypot(dx, dz)
        dx, dz = dx / length, dz / length
        x += dx * ds
        z += dz * ds
        minimum = min(minimum, z)
        if z < 0.0:
            return None
    return z, minimum, math.atan2(dz, dx)


for gradient in (-10.0, -15.0, -20.0, -30.0, -45.0, -60.0, -90.0):
    G0 = gradient
    target_x = 1800.0
    solutions = []
    for sample in range(1, 8001):
        angle = -sample * 0.0000025
        result = trace(angle, target_x)
        if result and 0.0 <= result[0] <= 15.0:
            solutions.append((angle, *result))
    if solutions:
        first, last = solutions[0], solutions[-1]
        arrivals = [s[1] for s in solutions]
        print(
            f"gradient={gradient:.0f}K/m launch={first[0]:.7f}..{last[0]:.7f}rad "
            f"arrival-span={min(arrivals):.3f}..{max(arrivals):.3f}m minimum={min(s[2] for s in solutions):.4f}m"
        )
    else:
        print(f"gradient={gradient:.0f}K/m no valid negative branch")

print("target sweep at -90 K/m")
G0 = -90.0
for target_x in (400.0, 600.0, 800.0, 1000.0, 1200.0, 1500.0, 1800.0, 2200.0, 2800.0):
    solutions = []
    for sample in range(1, 12001):
        angle = -sample * 0.0000025
        result = trace(angle, target_x)
        if result and 0.0 <= result[0] <= 20.0:
            solutions.append((angle, *result))
    if solutions:
        arrivals = [s[1] for s in solutions]
        print(f"target={target_x:.0f}m launch={solutions[0][0]:.6f}..{solutions[-1][0]:.6f} "
              f"arrival={min(arrivals):.2f}..{max(arrivals):.2f}m")

