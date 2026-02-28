"""
Geometry mesh helpers — generate Plotly Mesh3d traces from JSON geometry definitions.
"""

import json
from pathlib import Path

import numpy as np
import plotly.graph_objects as go


# ---------------------------------------------------------------------------
#  Colour helpers
# ---------------------------------------------------------------------------

def rgba_str(color, alpha_override=None):
    """Convert [r, g, b, a] to 'rgba(…)' string."""
    if not color or len(color) < 3:
        return "rgba(150,150,150,0.25)"
    r, g, b = [int(c * 255) if c <= 1 else int(c) for c in color[:3]]
    a = alpha_override if alpha_override is not None else (color[3] if len(color) > 3 else 0.3)
    return f"rgba({r},{g},{b},{a})"


# ---------------------------------------------------------------------------
#  Rotation
# ---------------------------------------------------------------------------

def rotation_matrix(rx, ry, rz):
    """Build a 3x3 rotation matrix from Euler angles in degrees (XYZ order)."""
    rx, ry, rz = np.radians(rx), np.radians(ry), np.radians(rz)
    cx, sx = np.cos(rx), np.sin(rx)
    cy, sy = np.cos(ry), np.sin(ry)
    cz, sz = np.cos(rz), np.sin(rz)
    Rx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    Ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    Rz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    return Rz @ Ry @ Rx


# ---------------------------------------------------------------------------
#  Mesh generators
# ---------------------------------------------------------------------------

def box_mesh(dx, dy, dz):
    """Return (vertices, i, j, k) for a box with half-sizes dx, dy, dz."""
    v = np.array([
        [-dx, -dy, -dz], [ dx, -dy, -dz], [ dx,  dy, -dz], [-dx,  dy, -dz],
        [-dx, -dy,  dz], [ dx, -dy,  dz], [ dx,  dy,  dz], [-dx,  dy,  dz],
    ], dtype=float)
    i = [0, 0, 4, 4, 0, 0, 2, 2, 0, 0, 1, 1]
    j = [1, 2, 5, 6, 1, 4, 3, 7, 3, 4, 2, 5]
    k = [2, 3, 6, 7, 5, 7, 7, 6, 4, 3, 6, 6]
    return v, i, j, k


def cylinder_mesh(radius, half_h, n_seg=24):
    """Return (vertices, i, j, k) for a cylinder along Z axis."""
    angles = np.linspace(0, 2 * np.pi, n_seg, endpoint=False)
    cos_a = np.cos(angles)
    sin_a = np.sin(angles)

    verts = []
    for z_sign in [-1, 1]:
        for c, s in zip(cos_a, sin_a):
            verts.append([radius * c, radius * s, z_sign * half_h])
    bottom_center = len(verts)
    verts.append([0, 0, -half_h])
    top_center = len(verts)
    verts.append([0, 0, half_h])
    verts = np.array(verts, dtype=float)

    ii, jj, kk = [], [], []
    for seg in range(n_seg):
        nxt = (seg + 1) % n_seg
        b0, b1 = seg, nxt
        t0, t1 = seg + n_seg, nxt + n_seg
        # Side face (2 triangles)
        ii += [b0, b1]; jj += [b1, t1]; kk += [t0, t0]
        # Bottom cap
        ii.append(bottom_center); jj.append(b1); kk.append(b0)
        # Top cap
        ii.append(top_center); jj.append(t0); kk.append(t1)

    return verts, ii, jj, kk


def sphere_mesh(radius, n_lat=12, n_lon=24):
    """Return (vertices, i, j, k) for a UV sphere."""
    verts = []
    for lat in range(n_lat + 1):
        theta = np.pi * lat / n_lat
        for lon in range(n_lon):
            phi = 2 * np.pi * lon / n_lon
            x = radius * np.sin(theta) * np.cos(phi)
            y = radius * np.sin(theta) * np.sin(phi)
            z = radius * np.cos(theta)
            verts.append([x, y, z])
    verts = np.array(verts, dtype=float)
    ii, jj, kk = [], [], []
    for lat in range(n_lat):
        for lon in range(n_lon):
            nlon = (lon + 1) % n_lon
            v0 = lat * n_lon + lon
            v1 = lat * n_lon + nlon
            v2 = (lat + 1) * n_lon + lon
            v3 = (lat + 1) * n_lon + nlon
            ii += [v0, v1]; jj += [v1, v3]; kk += [v2, v2]
    return verts, ii, jj, kk


# ---------------------------------------------------------------------------
#  High-level helper — add geometry traces to a Plotly figure
# ---------------------------------------------------------------------------

def add_geometry_traces(fig, geom: dict, materials: dict) -> None:
    """Add semi-transparent Mesh3d traces for each visible volume in *geom*."""
    volumes = geom.get("volumes", [])

    for vol in volumes:
        if not vol.get("visible", True):
            continue

        vtype    = vol.get("type", "")
        dims     = vol.get("dimensions", {})
        mat_name = vol.get("material", "")
        mat      = materials.get(mat_name, {})
        color    = mat.get("color", [0.6, 0.6, 0.6, 0.3])

        # Generate mesh vertices for the shape
        if vtype == "box":
            verts, ti, tj, tk = box_mesh(
                dims.get("x", 0) / 2.0,
                dims.get("y", 0) / 2.0,
                dims.get("z", 0) / 2.0,
            )
        elif vtype == "cylinder":
            verts, ti, tj, tk = cylinder_mesh(
                dims.get("radius", 0),
                dims.get("height", 0) / 2.0,
            )
        elif vtype == "sphere":
            verts, ti, tj, tk = sphere_mesh(dims.get("radius", 0))
        else:
            # Skip unsupported types (union, subtraction, etc.)
            continue

        # Place each copy of the volume
        for pl in vol.get("placements", []):
            px  = pl.get("x", 0)
            py  = pl.get("y", 0)
            pz  = pl.get("z", 0)
            rot = pl.get("rotation", {})
            rx  = rot.get("x", 0)
            ry  = rot.get("y", 0)
            rz  = rot.get("z", 0)

            placed = verts.copy()
            if rx != 0 or ry != 0 or rz != 0:
                R = rotation_matrix(rx, ry, rz)
                placed = (R @ placed.T).T
            placed[:, 0] += px
            placed[:, 1] += py
            placed[:, 2] += pz

            vol_name = vol.get("g4name") or vol.get("name", vtype)
            fig.add_trace(go.Mesh3d(
                x=placed[:, 0].tolist(),
                y=placed[:, 1].tolist(),
                z=placed[:, 2].tolist(),
                i=ti, j=tj, k=tk,
                color=rgba_str(color, 0.15),
                opacity=0.20,
                flatshading=True,
                name=f"geom: {vol_name}",
                showlegend=True,
                hoverinfo="name",
            ))
