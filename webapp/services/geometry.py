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

def _Rx(a):
    c, s = np.cos(a), np.sin(a)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])

def _Ry(a):
    c, s = np.cos(a), np.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])

def _Rz(a):
    c, s = np.cos(a), np.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])


def rotation_matrix_placement(rx, ry, rz):
    """Rotation for regular G4PVPlacement volumes.

    Angles are in **radians** (as stored in the JSON).

    Geant4's GeometryParser negates the angles and builds
    ``M = Rz(-rz) @ Ry(-ry) @ Rx(-rx)`` which is passed to
    G4PVPlacement as a *frame* (passive) rotation.  The physical
    daughter-to-mother transform is ``M^{-1} = Rx(rx) @ Ry(ry) @ Rz(rz)``.
    """
    return _Rx(rx) @ _Ry(ry) @ _Rz(rz)


def rotation_matrix_assembly(rx, ry, rz):
    """Rotation for assembly / MakeImprint placements.

    Angles are in **radians**.  MakeImprint applies the rotation matrix
    as an active transform without negation, so the matrix is
    ``Rz @ Ry @ Rx`` (same composition order as rotateX→Y→Z with
    pre-multiplication).
    """
    return _Rz(rz) @ _Ry(ry) @ _Rx(rx)


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
#  Mesh generation for a given shape type
# ---------------------------------------------------------------------------

def _generate_mesh(vtype, dims):
    """Return (vertices, i, j, k) for a primitive shape, or None."""
    if vtype == "box":
        return box_mesh(
            dims.get("x", 0) / 2.0,
            dims.get("y", 0) / 2.0,
            dims.get("z", 0) / 2.0,
        )
    elif vtype == "cylinder":
        return cylinder_mesh(
            dims.get("radius", 0),
            dims.get("height", 0) / 2.0,
        )
    elif vtype == "sphere":
        return sphere_mesh(dims.get("radius", 0))
    return None


def _add_mesh_trace(fig, verts, ti, tj, tk, R_total, t_total, color, vol_name):
    """Apply a composed rotation + translation and add a Mesh3d trace."""
    placed = (R_total @ verts.T).T + t_total
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


# ---------------------------------------------------------------------------
#  Assembly handling
# ---------------------------------------------------------------------------

def _add_assembly_traces(fig, assembly_vol, materials):
    """Draw every visible component of an assembly volume."""
    components = assembly_vol.get("components", [])
    if not components:
        return

    # Lookup from component name → component dict
    comp_map = {comp["name"]: comp for comp in components}

    for asm_pl in assembly_vol.get("placements", []):
        # Assembly-level placement transform (MakeImprint convention)
        asm_rot = asm_pl.get("rotation", {})
        asm_rx = asm_rot.get("x", 0)
        asm_ry = asm_rot.get("y", 0)
        asm_rz = asm_rot.get("z", 0)
        asm_R = rotation_matrix_assembly(asm_rx, asm_ry, asm_rz) if (asm_rx or asm_ry or asm_rz) else np.eye(3)
        asm_t = np.array([asm_pl.get("x", 0), asm_pl.get("y", 0), asm_pl.get("z", 0)])

        for comp in components:
            if not comp.get("visible", True):
                continue

            comp_type = comp.get("type", "")
            mesh = _generate_mesh(comp_type, comp.get("dimensions", {}))
            if mesh is None:
                continue
            verts, ti, tj, tk = mesh

            mat_name = comp.get("material", "")
            mat = materials.get(mat_name, {})
            color = mat.get("color", [0.6, 0.6, 0.6, 0.3])
            vol_name = comp.get("g4name") or comp.get("name", comp_type)

            for comp_pl in comp.get("placements", []):
                # Walk the parent chain from this component up to the
                # assembly root to collect all intermediate transforms.
                chain = []  # from component → parent → … → root
                current_pl = comp_pl
                while True:
                    chain.append(current_pl)
                    parent_name = current_pl.get("parent", "")
                    if not parent_name or parent_name not in comp_map:
                        break
                    parent_comp = comp_map[parent_name]
                    current_pl = parent_comp.get("placements", [{}])[0]

                # Compose transforms from outermost (assembly) to innermost
                # (component).  Update rule: given accumulated (R_acc, t_acc)
                # and next inner level (R_loc, t_loc), the new composed
                # transform that maps local vertices to world coords is:
                #   t_acc' = R_acc @ t_loc + t_acc
                #   R_acc' = R_acc @ R_loc
                R_acc = asm_R.copy()
                t_acc = asm_t.copy()
                for pl in reversed(chain):
                    rot = pl.get("rotation", {})
                    rx = rot.get("x", 0)
                    ry = rot.get("y", 0)
                    rz = rot.get("z", 0)
                    # Assembly component placements use the same active
                    # convention as the assembly itself (MakeImprint).
                    R_loc = rotation_matrix_assembly(rx, ry, rz) if (rx or ry or rz) else np.eye(3)
                    t_loc = np.array([pl.get("x", 0), pl.get("y", 0), pl.get("z", 0)])
                    t_acc = R_acc @ t_loc + t_acc
                    R_acc = R_acc @ R_loc

                _add_mesh_trace(fig, verts, ti, tj, tk, R_acc, t_acc, color, vol_name)


# ---------------------------------------------------------------------------
#  High-level helper — add geometry traces to a Plotly figure
# ---------------------------------------------------------------------------

def add_geometry_traces(fig, geom: dict, materials: dict) -> None:
    """Add semi-transparent Mesh3d traces for each visible volume in *geom*."""
    volumes = geom.get("volumes", [])

    for vol in volumes:
        if not vol.get("visible", True):
            continue

        vtype = vol.get("type", "")

        # ── Assembly / compound volumes ──────────────────────
        if vtype == "assembly":
            _add_assembly_traces(fig, vol, materials)
            continue

        # ── Primitive shapes ─────────────────────────────────
        dims     = vol.get("dimensions", {})
        mat_name = vol.get("material", "")
        mat      = materials.get(mat_name, {})
        color    = mat.get("color", [0.6, 0.6, 0.6, 0.3])

        mesh = _generate_mesh(vtype, dims)
        if mesh is None:
            # Skip unsupported types (union, subtraction, etc.)
            continue
        verts, ti, tj, tk = mesh

        # Place each copy of the volume
        for pl in vol.get("placements", []):
            px  = pl.get("x", 0)
            py  = pl.get("y", 0)
            pz  = pl.get("z", 0)
            rot = pl.get("rotation", {})
            rx  = rot.get("x", 0)
            ry  = rot.get("y", 0)
            rz  = rot.get("z", 0)

            R = rotation_matrix_placement(rx, ry, rz) if (rx or ry or rz) else np.eye(3)
            t = np.array([px, py, pz])
            _add_mesh_trace(fig, verts, ti, tj, tk, R, t, color,
                            vol.get("g4name") or vol.get("name", vtype))
