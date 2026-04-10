"""
Geometry mesh helpers — generate Plotly Mesh3d traces from JSON geometry definitions.

Uses trimesh + manifold3d for true CSG boolean operations (union, subtraction,
intersection) so that boolean solids render as a single watertight mesh.
"""

import json
from pathlib import Path

import numpy as np
import plotly.graph_objects as go
import trimesh


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
#  Parent-chain world transform
# ---------------------------------------------------------------------------

def _get_world_transform(vol_name, volumes_by_name, cache):
    """Return (R_world, t_world) for *vol_name* by walking the parent chain.

    Uses ``rotation_matrix_placement`` because regular volumes are placed via
    G4PVPlacement.  Results are memoised in *cache*.
    """
    if vol_name in cache:
        return cache[vol_name]
    if vol_name == "World" or vol_name not in volumes_by_name:
        cache[vol_name] = (np.eye(3), np.zeros(3))
        return cache[vol_name]
    vol = volumes_by_name[vol_name]
    pls = vol.get("placements", [])
    if not pls:
        cache[vol_name] = (np.eye(3), np.zeros(3))
        return cache[vol_name]
    pl = pls[0]
    rot = pl.get("rotation", {})
    rx, ry, rz = rot.get("x", 0), rot.get("y", 0), rot.get("z", 0)
    R_loc = rotation_matrix_placement(rx, ry, rz) if (rx or ry or rz) else np.eye(3)
    t_loc = np.array([pl.get("x", 0), pl.get("y", 0), pl.get("z", 0)])
    parent_name = pl.get("parent", "World")
    R_parent, t_parent = _get_world_transform(parent_name, volumes_by_name, cache)
    R_world = R_parent @ R_loc
    t_world = R_parent @ t_loc + t_parent
    cache[vol_name] = (R_world, t_world)
    return cache[vol_name]


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

def ellipsoid_mesh(rx, ry, rz, n_lat=12, n_lon=24):
    """Return (vertices, i, j, k) for an ellipsoid with semi-axes rx, ry, rz."""
    verts = []
    for lat in range(n_lat + 1):
        theta = np.pi * lat / n_lat
        for lon in range(n_lon):
            phi = 2 * np.pi * lon / n_lon
            x = rx * np.sin(theta) * np.cos(phi)
            y = ry * np.sin(theta) * np.sin(phi)
            z = rz * np.cos(theta)
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


def torus_mesh(r_major, r_minor, n_major=32, n_minor=16):
    """Return (vertices, i, j, k) for a torus with given radii."""
    verts = []
    for i in range(n_major):
        phi = 2 * np.pi * i / n_major
        for j in range(n_minor):
            theta = 2 * np.pi * j / n_minor
            x = (r_major + r_minor * np.cos(theta)) * np.cos(phi)
            y = (r_major + r_minor * np.cos(theta)) * np.sin(phi)
            z = r_minor * np.sin(theta)
            verts.append([x, y, z])
    verts = np.array(verts, dtype=float)
    ii, jj, kk = [], [], []
    for i in range(n_major):
        ni = (i + 1) % n_major
        for j in range(n_minor):
            nj = (j + 1) % n_minor
            v0 = i * n_minor + j
            v1 = i * n_minor + nj
            v2 = ni * n_minor + j
            v3 = ni * n_minor + nj
            ii += [v0, v1]; jj += [v1, v3]; kk += [v2, v2]
    return verts, ii, jj, kk


def trapezoid_mesh(dx1, dx2, dy1, dy2, dz):
    """Return (vertices, i, j, k) for a G4Trd trapezoid.

    dx1, dx2 are half-lengths in X at -dz and +dz.
    dy1, dy2 are half-lengths in Y at -dz and +dz.
    dz is the half-length in Z.
    """
    # 8 vertices: bottom face at z=-dz, top face at z=+dz
    verts = np.array([
        [-dx1, -dy1, -dz],  # 0  bottom
        [ dx1, -dy1, -dz],  # 1
        [ dx1,  dy1, -dz],  # 2
        [-dx1,  dy1, -dz],  # 3
        [-dx2, -dy2,  dz],  # 4  top
        [ dx2, -dy2,  dz],  # 5
        [ dx2,  dy2,  dz],  # 6
        [-dx2,  dy2,  dz],  # 7
    ], dtype=float)
    # 12 triangles (2 per face, 6 faces)
    ii = [0, 0, 4, 4, 0, 0, 1, 1, 0, 0, 2, 2]
    jj = [1, 2, 5, 6, 1, 4, 2, 5, 3, 4, 3, 6]
    kk = [2, 3, 6, 7, 4, 5, 5, 6, 4, 7, 7, 7]
    return verts, ii, jj, kk


def polycone_mesh(z_vals, rmin_vals, rmax_vals, n_seg=24):
    """Return (vertices, i, j, k) for a polycone along Z axis."""
    n_planes = len(z_vals)
    if n_planes < 2:
        return None
    angles = np.linspace(0, 2 * np.pi, n_seg, endpoint=False)
    cos_a = np.cos(angles)
    sin_a = np.sin(angles)

    verts = []
    # Outer surface vertices, then inner surface vertices
    for surface_radii in [rmax_vals, rmin_vals]:
        for plane_idx in range(n_planes):
            r = surface_radii[plane_idx] if surface_radii else 0
            z = z_vals[plane_idx]
            for c, s in zip(cos_a, sin_a):
                verts.append([r * c, r * s, z])
    verts = np.array(verts, dtype=float)

    ii, jj, kk = [], [], []
    # Outer surface quads
    for plane_idx in range(n_planes - 1):
        for seg in range(n_seg):
            nxt = (seg + 1) % n_seg
            b0 = plane_idx * n_seg + seg
            b1 = plane_idx * n_seg + nxt
            t0 = (plane_idx + 1) * n_seg + seg
            t1 = (plane_idx + 1) * n_seg + nxt
            ii += [b0, b1]; jj += [b1, t1]; kk += [t0, t0]

    has_inner = rmin_vals and any(r > 0 for r in rmin_vals)
    inner_offset = n_planes * n_seg

    if has_inner:
        # Inner surface quads
        for plane_idx in range(n_planes - 1):
            for seg in range(n_seg):
                nxt = (seg + 1) % n_seg
                b0 = inner_offset + plane_idx * n_seg + seg
                b1 = inner_offset + plane_idx * n_seg + nxt
                t0 = inner_offset + (plane_idx + 1) * n_seg + seg
                t1 = inner_offset + (plane_idx + 1) * n_seg + nxt
                ii += [b0, b1]; jj += [b1, t1]; kk += [t0, t0]

    # End caps (connect outer to inner at first and last plane)
    for plane_idx in [0, n_planes - 1]:
        for seg in range(n_seg):
            nxt = (seg + 1) % n_seg
            o0 = plane_idx * n_seg + seg
            o1 = plane_idx * n_seg + nxt
            if has_inner:
                i0 = inner_offset + plane_idx * n_seg + seg
                i1 = inner_offset + plane_idx * n_seg + nxt
                ii += [o0, o1]; jj += [o1, i1]; kk += [i0, i0]
            else:
                # Solid endcap — fan from first vertex
                if seg >= 2:
                    ii.append(plane_idx * n_seg)
                    jj.append(plane_idx * n_seg + seg - 1)
                    kk.append(plane_idx * n_seg + seg)

    return verts, ii, jj, kk


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
    elif vtype == "ellipsoid":
        return ellipsoid_mesh(
            dims.get("x_radius", 0),
            dims.get("y_radius", 0),
            dims.get("z_radius", 0),
        )
    elif vtype == "torus":
        return torus_mesh(
            dims.get("major_radius", dims.get("radius", 0)),
            dims.get("minor_radius", dims.get("tube_radius", 0)),
        )
    elif vtype == "trapezoid" or vtype == "trd":
        return trapezoid_mesh(
            dims.get("dx1", 0) / 2.0,
            dims.get("dx2", 0) / 2.0,
            dims.get("dy1", 0) / 2.0,
            dims.get("dy2", 0) / 2.0,
            dims.get("dz", 0),
        )
    elif vtype == "polycone":
        z_vals = dims.get("z", [])
        rmin_vals = dims.get("rmin", [])
        rmax_vals = dims.get("rmax", [])
        if z_vals and rmax_vals:
            return polycone_mesh(z_vals, rmin_vals, rmax_vals)
        return None
    return None


def _add_mesh_trace(fig, verts, ti, tj, tk, R_total, t_total, color, vol_name):
    """Apply a composed rotation + translation and add a Mesh3d trace."""
    placed = (R_total @ verts.T).T + t_total
    fig.add_trace(go.Mesh3d(
        x=placed[:, 0].tolist(),
        y=placed[:, 1].tolist(),
        z=placed[:, 2].tolist(),
        i=ti, j=tj, k=tk,
        color=rgba_str(color, 0.4),
        opacity=0.5,
        flatshading=True,
        name=f"geom: {vol_name}",
        showlegend=True,
        hoverinfo="name",
    ))


# ---------------------------------------------------------------------------
#  Assembly handling
# ---------------------------------------------------------------------------

def _add_assembly_traces(fig, assembly_vol, materials,
                         R_parent=None, t_parent=None):
    """Draw every visible component of an assembly volume."""
    if R_parent is None:
        R_parent = np.eye(3)
    if t_parent is None:
        t_parent = np.zeros(3)

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
        asm_R_loc = rotation_matrix_assembly(asm_rx, asm_ry, asm_rz) if (asm_rx or asm_ry or asm_rz) else np.eye(3)
        asm_t_loc = np.array([asm_pl.get("x", 0), asm_pl.get("y", 0), asm_pl.get("z", 0)])
        # Compose with parent world transform
        asm_R = R_parent @ asm_R_loc
        asm_t = R_parent @ asm_t_loc + t_parent

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
#  Boolean solid handling (union / subtraction / intersection)
#  Uses trimesh + manifold3d for true CSG mesh operations.
# ---------------------------------------------------------------------------

def _generate_trimesh(vtype, dims):
    """Create a watertight trimesh.Trimesh for a primitive shape type."""
    if vtype == "box":
        dx = dims.get("x", 0)
        dy = dims.get("y", 0)
        dz = dims.get("z", 0)
        return trimesh.creation.box(extents=[dx, dy, dz])
    elif vtype == "cylinder":
        radius = dims.get("radius", 0)
        height = dims.get("height", 0)
        # trimesh cylinder along Z by default
        return trimesh.creation.cylinder(radius=radius, height=height, sections=48)
    elif vtype == "sphere":
        radius = dims.get("radius", 0)
        return trimesh.creation.icosphere(subdivisions=3, radius=radius)
    elif vtype == "ellipsoid":
        # Create a unit sphere and scale
        rx = dims.get("x_radius", 1)
        ry = dims.get("y_radius", 1)
        rz = dims.get("z_radius", 1)
        m = trimesh.creation.icosphere(subdivisions=3, radius=1.0)
        m.vertices *= np.array([rx, ry, rz])
        return m
    elif vtype == "torus":
        r_major = dims.get("major_radius", dims.get("radius", 0))
        r_minor = dims.get("minor_radius", dims.get("tube_radius", 0))
        # trimesh doesn't have a torus primitive; build from revolution
        # Use a circle revolved around the Z axis
        angles = np.linspace(0, 2 * np.pi, 32, endpoint=False)
        section = np.column_stack([
            r_minor * np.cos(angles) + r_major,
            r_minor * np.sin(angles),
        ])
        path = trimesh.path.Path2D(
            entities=[trimesh.path.entities.Line(list(range(len(section))) + [0])],
            vertices=section,
        )
        return trimesh.creation.revolve(path, sections=48)
    elif vtype == "trapezoid" or vtype == "trd":
        dx1 = dims.get("dx1", 0) / 2.0
        dx2 = dims.get("dx2", 0) / 2.0
        dy1 = dims.get("dy1", 0) / 2.0
        dy2 = dims.get("dy2", 0) / 2.0
        dz  = dims.get("dz", 0)
        verts_arr = np.array([
            [-dx1, -dy1, -dz],
            [ dx1, -dy1, -dz],
            [ dx1,  dy1, -dz],
            [-dx1,  dy1, -dz],
            [-dx2, -dy2,  dz],
            [ dx2, -dy2,  dz],
            [ dx2,  dy2,  dz],
            [-dx2,  dy2,  dz],
        ])
        faces = np.array([
            [0, 1, 2], [0, 2, 3],  # bottom
            [4, 6, 5], [4, 7, 6],  # top
            [0, 4, 5], [0, 5, 1],  # front
            [1, 5, 6], [1, 6, 2],  # right
            [2, 6, 7], [2, 7, 3],  # back
            [3, 7, 4], [3, 4, 0],  # left
        ])
        return trimesh.Trimesh(vertices=verts_arr, faces=faces)
    return None


def _compose_transform(R, t):
    """Build a 4×4 homogeneous transform from a 3×3 rotation and 3-vector."""
    T = np.eye(4)
    T[:3, :3] = R
    T[:3, 3] = t
    return T


def _add_boolean_traces(fig, bool_vol, materials,
                        volumes_by_name=None, transform_cache=None):
    """Compute true CSG boolean of components and render the result mesh.

    Uses trimesh + manifold3d to perform real union / subtraction /
    intersection operations, producing a single clean mesh per placement.
    Falls back to drawing individual components if CSG fails.
    """
    if volumes_by_name is None:
        volumes_by_name = {}
    if transform_cache is None:
        transform_cache = {}
    components = bool_vol.get("components", [])
    if not components:
        return

    mat_name = bool_vol.get("material", "")
    mat = materials.get(mat_name, {})
    default_color = mat.get("color", [0.6, 0.6, 0.6, 0.3])
    vol_name = bool_vol.get("g4name") or bool_vol.get("name", "boolean")

    # ── Build the CSG result in the boolean solid's local frame ──────
    # Separate union and subtraction components (matching Geant4 order:
    # unions first, then subtractions).
    union_meshes = []
    subtract_meshes = []

    for comp in components:
        comp_type = comp.get("type", "")
        tm = _generate_trimesh(comp_type, comp.get("dimensions", {}))
        if tm is None:
            continue

        # Apply placement transform (relative to boolean solid origin)
        for comp_pl in comp.get("placements", [{}]):
            crot = comp_pl.get("rotation", {})
            crx = crot.get("x", 0)
            cry = crot.get("y", 0)
            crz = crot.get("z", 0)
            R_comp = rotation_matrix_placement(crx, cry, crz) if (crx or cry or crz) else np.eye(3)
            t_comp = np.array([comp_pl.get("x", 0), comp_pl.get("y", 0), comp_pl.get("z", 0)])
            placed = tm.copy()
            placed.apply_transform(_compose_transform(R_comp, t_comp))

            bool_op = comp.get("boolean_operation", "union")
            if bool_op == "subtract":
                subtract_meshes.append(placed)
            else:
                union_meshes.append(placed)

    if not union_meshes:
        return

    # ── Perform CSG ──────────────────────────────────────────────────
    try:
        # Union all "add" components
        if len(union_meshes) == 1:
            result = union_meshes[0]
        else:
            result = trimesh.boolean.union(union_meshes, engine="manifold")

        # Subtract each "subtract" component
        for sub in subtract_meshes:
            result = trimesh.boolean.difference([result, sub], engine="manifold")
    except Exception:
        # Fallback: draw components individually (old behaviour)
        _add_boolean_traces_fallback(fig, bool_vol, materials,
                                         volumes_by_name, transform_cache)
        return

    if result is None or len(result.vertices) == 0:
        return

    # ── Convert to Plotly and place in world ─────────────────────────
    verts = np.array(result.vertices)
    faces = np.array(result.faces)
    ti = faces[:, 0].tolist()
    tj = faces[:, 1].tolist()
    tk = faces[:, 2].tolist()

    # Determine colour from the first union component's material
    first_mat = components[0].get("material", mat_name)
    first_mat_info = materials.get(first_mat, mat)
    color = first_mat_info.get("color", default_color)

    for vol_pl in bool_vol.get("placements", []):
        vol_rot = vol_pl.get("rotation", {})
        vrx = vol_rot.get("x", 0)
        vry = vol_rot.get("y", 0)
        vrz = vol_rot.get("z", 0)
        R_loc = rotation_matrix_placement(vrx, vry, vrz) if (vrx or vry or vrz) else np.eye(3)
        t_loc = np.array([vol_pl.get("x", 0), vol_pl.get("y", 0), vol_pl.get("z", 0)])

        parent_name = vol_pl.get("parent", "World")
        R_par, t_par = _get_world_transform(
            parent_name, volumes_by_name, transform_cache)
        R_vol = R_par @ R_loc
        t_vol = R_par @ t_loc + t_par

        _add_mesh_trace(fig, verts, ti, tj, tk, R_vol, t_vol, color, vol_name)


def _add_boolean_traces_fallback(fig, bool_vol, materials,
                                 volumes_by_name=None, transform_cache=None):
    """Fallback: draw each component separately when CSG fails."""
    if volumes_by_name is None:
        volumes_by_name = {}
    if transform_cache is None:
        transform_cache = {}
    components = bool_vol.get("components", [])
    mat_name = bool_vol.get("material", "")
    mat = materials.get(mat_name, {})
    default_color = mat.get("color", [0.6, 0.6, 0.6, 0.3])

    for vol_pl in bool_vol.get("placements", []):
        vol_rot = vol_pl.get("rotation", {})
        vrx = vol_rot.get("x", 0)
        vry = vol_rot.get("y", 0)
        vrz = vol_rot.get("z", 0)
        R_loc = rotation_matrix_placement(vrx, vry, vrz) if (vrx or vry or vrz) else np.eye(3)
        t_loc = np.array([vol_pl.get("x", 0), vol_pl.get("y", 0), vol_pl.get("z", 0)])

        parent_name = vol_pl.get("parent", "World")
        R_par, t_par = _get_world_transform(
            parent_name, volumes_by_name, transform_cache)
        R_vol = R_par @ R_loc
        t_vol = R_par @ t_loc + t_par

        for comp in components:
            if not comp.get("visible", True):
                continue
            comp_type = comp.get("type", "")
            mesh = _generate_mesh(comp_type, comp.get("dimensions", {}))
            if mesh is None:
                continue
            verts, ti, tj, tk = mesh

            bool_op = comp.get("boolean_operation", "union")
            if bool_op == "subtract":
                color = [0.9, 0.2, 0.2, 0.25]
            else:
                comp_mat = comp.get("material", mat_name)
                comp_mat_info = materials.get(comp_mat, mat)
                color = comp_mat_info.get("color", default_color)

            name = comp.get("g4name") or comp.get("name", comp_type)

            for comp_pl in comp.get("placements", []):
                crot = comp_pl.get("rotation", {})
                crx = crot.get("x", 0)
                cry = crot.get("y", 0)
                crz = crot.get("z", 0)
                R_comp = rotation_matrix_placement(crx, cry, crz) if (crx or cry or crz) else np.eye(3)
                t_comp = np.array([comp_pl.get("x", 0), comp_pl.get("y", 0), comp_pl.get("z", 0)])
                R_total = R_vol @ R_comp
                t_total = R_vol @ t_comp + t_vol
                _add_mesh_trace(fig, verts, ti, tj, tk, R_total, t_total, color,
                                name + (" [subtract]" if bool_op == "subtract" else ""))


# ---------------------------------------------------------------------------
#  High-level helper — add geometry traces to a Plotly figure
# ---------------------------------------------------------------------------

def add_geometry_traces(fig, geom: dict, materials: dict) -> None:
    """Add semi-transparent Mesh3d traces for each visible volume in *geom*."""
    volumes = geom.get("volumes", [])

    # Build name → volume lookup for parent-chain resolution
    volumes_by_name = {v["name"]: v for v in volumes if "name" in v}
    transform_cache = {}                # vol_name → (R_world, t_world)

    for vol in volumes:
        if not vol.get("visible", True):
            continue

        vtype = vol.get("type", "")

        # ── Assembly / compound volumes ──────────────────────
        if vtype == "assembly":
            # Determine parent world transform from the first placement
            pls = vol.get("placements", [])
            parent_name = pls[0].get("parent", "World") if pls else "World"
            R_par, t_par = _get_world_transform(
                parent_name, volumes_by_name, transform_cache)
            _add_assembly_traces(fig, vol, materials, R_par, t_par)
            continue

        # ── Boolean solids (union, subtraction, intersection) ─
        if vtype in ("union", "subtraction", "intersection"):
            _add_boolean_traces(fig, vol, materials,
                                volumes_by_name, transform_cache)
            continue

        # ── Primitive shapes ─────────────────────────────────
        dims     = vol.get("dimensions", {})
        mat_name = vol.get("material", "")
        mat      = materials.get(mat_name, {})
        color    = mat.get("color", [0.6, 0.6, 0.6, 0.3])

        mesh = _generate_mesh(vtype, dims)
        if mesh is None:
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

            R_loc = rotation_matrix_placement(rx, ry, rz) if (rx or ry or rz) else np.eye(3)
            t_loc = np.array([px, py, pz])

            # Compose with parent world transform
            parent_name = pl.get("parent", "World")
            R_par, t_par = _get_world_transform(
                parent_name, volumes_by_name, transform_cache)
            R = R_par @ R_loc
            t = R_par @ t_loc + t_par

            _add_mesh_trace(fig, verts, ti, tj, tk, R, t, color,
                            vol.get("g4name") or vol.get("name", vtype))
