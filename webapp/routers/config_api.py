"""
Configuration API — geometry file listing and upload.
"""

import json

from fastapi import APIRouter, Request

from config import CONFIG_DIR

router = APIRouter(prefix="/api", tags=["config"])


@router.get("/geometries")
async def list_geometries():
    """Return available geometry JSON files in config/."""
    files = sorted(CONFIG_DIR.glob("*.json"))
    return [f.name for f in files]


@router.post("/upload-geometry")
async def upload_geometry(request: Request):
    """Upload a geometry JSON and save it to config/."""
    body = await request.json()
    name = body.get("name", "uploaded.json")
    if not name.endswith(".json"):
        name += ".json"
    data = body.get("data")
    dest = CONFIG_DIR / name
    with open(dest, "w") as f:
        json.dump(data, f, indent=2)
    return {"status": "ok", "file": name}
