"""
Configuration API — geometry file listing, upload, volume introspection, and 3-D preview.
"""

import json

import plotly.graph_objects as go
from fastapi import APIRouter, Request
from fastapi.responses import JSONResponse

from config import CONFIG_DIR
from services.geometry import add_geometry_traces

router = APIRouter(prefix="/api", tags=["config"])


@router.get("/geometries")
async def list_geometries():
    """Return available geometry JSON files in config/."""
    files = sorted(CONFIG_DIR.glob("*.json"))
    return [f.name for f in files]


@router.get("/geometries/{filename}/volumes")
async def list_volumes(filename: str):
    """Return the physical volume names defined in a geometry JSON.

    Walks the volumes array and extracts the g4name from each placement.
    Falls back to the volume-level g4name or the volume name.
    """
    path = (CONFIG_DIR / filename).resolve()
    if not str(path).startswith(str(CONFIG_DIR.resolve())) or not path.is_file():
        return JSONResponse({"error": "Geometry file not found"}, status_code=404)

    with open(path) as f:
        geo = json.load(f)

    names: list[str] = []

    # World volume
    world = geo.get("world", {})
    w_name = world.get("g4name") or world.get("name") or "World"
    names.append(w_name)

    # Child volumes
    for vol in geo.get("volumes", []):
        vol_g4 = vol.get("g4name") or vol.get("name", "")
        placements = vol.get("placements", [])
        if placements:
            for pl in placements:
                pname = pl.get("g4name") or vol_g4
                if pname and pname not in names:
                    names.append(pname)
        elif vol_g4 and vol_g4 not in names:
            names.append(vol_g4)

    return names


@router.post("/upload-geometry")
async def upload_geometry(request: Request):
    """Upload a geometry JSON and save it to config/."""
    body = await request.json()
    name = body.get("name", "uploaded.json")
    if not name.endswith(".json"):
        name += ".json"
    data = body.get("data")

    # ── Path-traversal guard ──────────────────────────────
    # Resolve the destination and ensure it stays inside CONFIG_DIR.
    dest = (CONFIG_DIR / name).resolve()
    if not str(dest).startswith(str(CONFIG_DIR.resolve())):
        return JSONResponse({"error": "Invalid filename"}, status_code=400)

    with open(dest, "w") as f:
        json.dump(data, f, indent=2)
    return {"status": "ok", "file": name}


@router.get("/geometries/{filename}/preview")
async def geometry_preview(filename: str):
    """Return Plotly JSON with geometry-only 3-D wireframes for a mini preview."""
    path = (CONFIG_DIR / filename).resolve()
    if not str(path).startswith(str(CONFIG_DIR.resolve())) or not path.is_file():
        return JSONResponse({"error": "Geometry file not found"}, status_code=404)

    with open(path) as f:
        geom = json.load(f)

    materials = geom.get("materials", {})
    fig = go.Figure()
    add_geometry_traces(fig, geom, materials)

    # Boost visibility for the small preview (default traces are very faint)
    for trace in fig.data:
        trace.opacity = 0.75

    # Determine axis extent from the world volume
    world = geom.get("world", {})
    w_dims = world.get("dimensions", {})
    half = max(w_dims.get("x", 200), w_dims.get("y", 200), w_dims.get("z", 200)) / 2.0
    axis_len = half * 0.5

    # Add thin axis lines (x=red, y=green, z=blue)
    for vec, color, label in [
        ([1, 0, 0], "rgba(220,40,40,0.6)", "x"),
        ([0, 1, 0], "rgba(40,160,40,0.6)", "y"),
        ([0, 0, 1], "rgba(40,80,220,0.6)", "z"),
    ]:
        fig.add_trace(go.Scatter3d(
            x=[0, vec[0] * axis_len],
            y=[0, vec[1] * axis_len],
            z=[0, vec[2] * axis_len],
            mode="lines+text",
            line=dict(color=color, width=3),
            text=["", label],
            textposition="top center",
            textfont=dict(size=10, color=color),
            showlegend=False,
            hoverinfo="skip",
        ))

    fig.update_layout(
        scene=dict(
            xaxis=dict(visible=False),
            yaxis=dict(visible=False),
            zaxis=dict(visible=False),
            aspectmode="data",
            bgcolor="rgba(0,0,0,0)",
        ),
        margin=dict(l=0, r=0, t=0, b=0),
        paper_bgcolor="rgba(0,0,0,0)",
        plot_bgcolor="rgba(0,0,0,0)",
        showlegend=False,
        height=280,
    )
    return {"plotJSON": fig.to_json()}
