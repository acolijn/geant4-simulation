"""
Results API — browse output files, download, plot histograms, and 3D hit maps.
"""

import json
from pathlib import Path

import numpy as np
import plotly.graph_objects as go
import uproot
from fastapi import APIRouter
from fastapi.responses import FileResponse, JSONResponse

from config import CONFIG_DIR, RUNS_DIR
from services.geometry import add_geometry_traces

router = APIRouter(prefix="/api/results", tags=["results"])


def _safe_run_path(*parts: str) -> "Path | None":
    """Resolve a path under RUNS_DIR and reject traversal attempts."""
    target = (RUNS_DIR.joinpath(*parts)).resolve()
    if not str(target).startswith(str(RUNS_DIR.resolve())):
        return None
    return target


def _find_root_files(run_dir: Path) -> list[Path]:
    """Find ROOT files — check root/ subdir first, then top-level (legacy)."""
    root_sub = run_dir / "root"
    if root_sub.is_dir():
        files = sorted(root_sub.glob("*.root"))
        if files:
            return files
    return sorted(run_dir.glob("*.root"))


@router.get("/{run_id}/files")
async def result_files(run_id: str):
    """List output files for a run, including files in subdirectories."""
    run_dir = _safe_run_path(run_id)
    if run_dir is None:
        return JSONResponse({"error": "Invalid run id"}, status_code=400)
    if not run_dir.exists():
        return JSONResponse({"error": "Run not found"}, status_code=404)
    result = []
    for f in run_dir.rglob("*"):
        if f.is_file() and f.suffix in (".root", ".mac", ".json", ".txt", ".out", ".err", ".log", ".sub"):
            result.append(str(f.relative_to(run_dir)))
    return sorted(result)


@router.get("/{run_id}/root-files")
async def root_files_list(run_id: str):
    """List ROOT files for a run (for the file selector dropdown)."""
    run_dir = _safe_run_path(run_id)
    if run_dir is None:
        return JSONResponse({"error": "Invalid run id"}, status_code=400)
    if not run_dir.exists():
        return JSONResponse({"error": "Run not found"}, status_code=404)
    files = _find_root_files(run_dir)
    return {"files": [f.name for f in files]}


@router.get("/{run_id}/download/{filename:path}")
async def download_file(run_id: str, filename: str):
    """Download a result file (supports subdir paths like root/G4sim.root)."""
    fpath = _safe_run_path(run_id, filename)
    if fpath is None:
        return JSONResponse({"error": "Invalid path"}, status_code=400)
    if not fpath.exists():
        return JSONResponse({"error": "File not found"}, status_code=404)
    return FileResponse(fpath, filename=Path(filename).name)


@router.get("/{run_id}/log")
async def get_log(run_id: str):
    """Return the log file for a completed run."""
    # Try new structure first (log/log.txt), then legacy (log.txt)
    log_path = _safe_run_path(run_id, "log", "log.txt")
    if log_path is None or not log_path.exists():
        log_path = _safe_run_path(run_id, "log.txt")
    if log_path is None:
        return JSONResponse({"error": "Invalid run id"}, status_code=400)
    if not log_path.exists():
        return JSONResponse({"error": "Log not found"}, status_code=404)
    return {"log": log_path.read_text()}


@router.get("/{run_id}/branches")
async def list_branches(run_id: str, file: str = ""):
    """List TTree branches in a ROOT output file.
    
    Query param 'file' selects a specific ROOT file; otherwise uses the first found.
    """
    run_dir = _safe_run_path(run_id)
    if run_dir is None:
        return JSONResponse({"error": "Invalid run id"}, status_code=400)
    
    if file:
        # Specific file requested
        root_path = (run_dir / "root" / file)
        if not root_path.exists():
            root_path = run_dir / file
        if not root_path.exists():
            return JSONResponse({"error": f"File {file} not found"}, status_code=404)
        root_files = [root_path]
    else:
        root_files = _find_root_files(run_dir)
    
    if not root_files:
        return JSONResponse({"error": "No ROOT file found"}, status_code=404)
    f = uproot.open(root_files[0])
    tree = f["events"]
    branches = list(tree.keys())
    return {"file": root_files[0].name, "branches": branches}


@router.get("/{run_id}/plot/{branch}")
async def plot_branch(run_id: str, branch: str, file: str = ""):
    """Return a Plotly JSON histogram for a given branch."""
    run_dir = _safe_run_path(run_id)
    if run_dir is None:
        return JSONResponse({"error": "Invalid run id"}, status_code=400)
    
    if file:
        root_path = (run_dir / "root" / file)
        if not root_path.exists():
            root_path = run_dir / file
        if not root_path.exists():
            return JSONResponse({"error": f"File {file} not found"}, status_code=404)
        root_files = [root_path]
    else:
        root_files = _find_root_files(run_dir)
    if not root_files:
        return JSONResponse({"error": "No ROOT file found"}, status_code=404)

    f = uproot.open(root_files[0])
    tree = f["events"]

    try:
        data = tree[branch].array(library="np")
    except Exception as e:
        return JSONResponse({"error": str(e)}, status_code=400)

    # Flatten if it's a jagged array
    if hasattr(data, "tolist"):
        flat = []
        for item in data:
            if hasattr(item, "__iter__") and not isinstance(item, str):
                flat.extend(item)
            else:
                flat.append(item)
        data = np.array(flat, dtype=float) if flat else np.array([])

    if len(data) == 0:
        return {"plotJSON": json.dumps({"data": [], "layout": {"title": f"{branch} (empty)"}})}

    fig = go.Figure(data=[go.Histogram(x=data.tolist(), nbinsx=100)])
    fig.update_layout(
        title=branch,
        xaxis_title=branch,
        yaxis_title="Counts",
        template="plotly_white",
        height=500,
        margin=dict(l=60, r=30, t=50, b=50),
    )
    return {"plotJSON": fig.to_json()}


@router.get("/{run_id}/plot3d")
async def plot_3d(run_id: str, file: str = ""):
    """Return Plotly JSON for a 3D scatter of hit positions overlaid with geometry wireframes."""
    run_dir = _safe_run_path(run_id)
    if run_dir is None:
        return JSONResponse({"error": "Invalid run id"}, status_code=400)
    
    if file:
        root_path = (run_dir / "root" / file)
        if not root_path.exists():
            root_path = run_dir / file
        if not root_path.exists():
            return JSONResponse({"error": f"File {file} not found"}, status_code=404)
        root_files = [root_path]
    else:
        root_files = _find_root_files(run_dir)
    if not root_files:
        return JSONResponse({"error": "No ROOT file found"}, status_code=404)

    f = uproot.open(root_files[0])
    tree = f["events"]
    keys = tree.keys()

    # Find detector prefixes by looking for *_x branches
    detectors = [k.replace("_x", "") for k in keys if k.endswith("_x")]

    fig = go.Figure()

    # ── Overlay geometry volumes ────────────────────────────
    meta_path = run_dir / "meta.json"
    if meta_path.exists():
        meta = json.loads(meta_path.read_text())
        geom_path = CONFIG_DIR / meta.get("geometry", "")
        if geom_path.exists():
            geom = json.loads(geom_path.read_text())
            materials = geom.get("materials", {})
            add_geometry_traces(fig, geom, materials)

    # ── Scatter plot of hits ────────────────────────────────
    for det in detectors:
        try:
            x = tree[f"{det}_x"].array(library="np")
            y = tree[f"{det}_y"].array(library="np")
            z = tree[f"{det}_z"].array(library="np")
            xf = np.concatenate([np.atleast_1d(a) for a in x]) if len(x) else np.array([])
            yf = np.concatenate([np.atleast_1d(a) for a in y]) if len(y) else np.array([])
            zf = np.concatenate([np.atleast_1d(a) for a in z]) if len(z) else np.array([])

            if len(xf) == 0:
                continue

            if len(xf) > 50000:
                idx = np.random.choice(len(xf), 50000, replace=False)
                xf, yf, zf = xf[idx], yf[idx], zf[idx]

            fig.add_trace(go.Scatter3d(
                x=xf.tolist(), y=yf.tolist(), z=zf.tolist(),
                mode="markers",
                marker=dict(size=1.5, opacity=0.6),
                name=det,
            ))
        except Exception:
            continue

    fig.update_layout(
        title="Hit positions + geometry (3D)",
        scene=dict(
            xaxis_title="x [mm]", yaxis_title="y [mm]", zaxis_title="z [mm]",
            aspectmode="data",
        ),
        template="plotly_white",
        height=600,
        margin=dict(l=0, r=0, t=40, b=0),
    )
    return {"plotJSON": fig.to_json()}
