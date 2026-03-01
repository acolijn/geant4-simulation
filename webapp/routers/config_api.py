"""
Configuration API — geometry file listing, upload, and volume introspection.
"""

import json

from fastapi import APIRouter, Request
from fastapi.responses import JSONResponse

from config import CONFIG_DIR

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
