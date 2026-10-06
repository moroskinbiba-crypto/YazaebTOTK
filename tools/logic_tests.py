#!/usr/bin/env python3
from math import isfinite, sqrt

# Pure-logic regression tests mirroring the runtime's coordinate conventions.

def display_from_engine(raw):
    x, y, z = raw
    return (x, z, y)

assert display_from_engine((-205.0, 35.0, 451.0)) == (-205.0, 451.0, 35.0)
assert display_from_engine((275.0, 1474.0, -913.0)) == (275.0, -913.0, 1474.0)

def distance2(a, b):
    dx = b[0] - a[0]
    dy = b[1] - a[1]
    dz = b[2] - a[2]
    return dx * dx + dy * dy + 0.25 * dz * dz

assert distance2((0, 0, 0), (3, 4, 0)) == 25
assert distance2((0, 0, 0), (0, 0, 20)) == 100
assert distance2((0, 0, -200), (0, 0, -190)) == 25

def layer_matches(point_layer, player_z):
    if not point_layer:
        return True
    if player_z < -100.0:
        return point_layer == "Depths"
    return point_layer != "Depths"

assert layer_matches("Depths", -150.0)
assert not layer_matches("Surface", -150.0)
assert not layer_matches("Sky", -150.0)
assert layer_matches("Surface", 35.0)
assert layer_matches("Sky", 35.0)

for value in (-12000.0, -1.0, 0.1, 12000.0):
    assert isfinite(value)

print("LOGIC TESTS OK")
print(" - engine/display coordinate transform")
print(" - horizontal-biased distance metric")
print(" - Depths vs Surface/Sky layer routing")
