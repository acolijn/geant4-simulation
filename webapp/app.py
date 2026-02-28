"""
G4sim Web Dashboard — FastAPI backend

A lightweight local web interface for configuring, running, and
inspecting Geant4 simulations.  Keeps the CLI workflow intact;
this is just a convenience layer on top of the same build/G4sim binary.
"""

import asyncio
import glob
import json
import os
import signal
import time
from datetime import datetime
from pathlib import Path

import plotly
import plotly.graph_objects as go
import uproot
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, Request
from fastapi.responses import FileResponse, HTMLResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates

# ---------------------------------------------------------------------------
#  Paths – everything is relative to the geant4-simulation project root
# ---------------------------------------------------------------------------
WEBAPP_DIR  = Path(__file__).resolve().parent
PROJECT_DIR = WEBAPP_DIR.parent
BUILD_DIR   = PROJECT_DIR / "build"
CONFIG_DIR  = PROJECT_DIR / "config"
RUNS_DIR    = WEBAPP_DIR / "runs"
G4SIM_BIN   = BUILD_DIR / "G4sim"

RUNS_DIR.mkdir(exist_ok=True)

# ---------------------------------------------------------------------------
#  App setup
# ---------------------------------------------------------------------------
app = FastAPI(title="G4sim Dashboard")
app.mount("/static", StaticFiles(directory=WEBAPP_DIR / "static"), name="static")
templates = Jinja2Templates(directory=WEBAPP_DIR / "templates")

# In-memory state for the currently running simulation
current_process: dict = {"proc": None, "info": None}


# ---------------------------------------------------------------------------
#  Pages
# ---------------------------------------------------------------------------
@app.get("/", response_class=HTMLResponse)
async def index(request: Request):
    return templates.TemplateResponse("index.html", {"request": request})


# ---------------------------------------------------------------------------
#  Config API
# ---------------------------------------------------------------------------
@app.get("/api/geometries")
async def list_geometries():
    """Return available geometry JSON files in config/."""
    files = sorted(CONFIG_DIR.glob("*.json"))
    return [f.name for f in files]


@app.post("/api/upload-geometry")
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


# ---------------------------------------------------------------------------
#  Run API
# ---------------------------------------------------------------------------
@app.post("/api/run")
async def start_run(request: Request):
    """Start a simulation run.  Expects JSON body with run parameters."""
    if current_process["proc"] is not None:
        return JSONResponse({"error": "A simulation is already running"}, status_code=409)

    body = await request.json()
    geometry   = body.get("geometry", "geometry.json")
    particle   = body.get("particle", "gamma")
    energy     = body.get("energy", "1")
    energy_unit = body.get("energyUnit", "MeV")
    pos_x      = body.get("posX", "-10")
    pos_y      = body.get("posY", "0")
    pos_z      = body.get("posZ", "0")
    pos_unit   = body.get("posUnit", "cm")
    dir_x      = body.get("dirX", "1")
    dir_y      = body.get("dirY", "0")
    dir_z      = body.get("dirZ", "0")
    n_events   = body.get("nEvents", "10000")
    output     = body.get("outputFile", "G4sim.root")
    verbose    = body.get("verboseHits", "0")

    # Create a unique run directory
    stamp    = datetime.now().strftime("%Y%m%d_%H%M%S")
    run_dir  = RUNS_DIR / stamp
    run_dir.mkdir(parents=True, exist_ok=True)

    # Generate the macro
    macro_path = run_dir / "run.mac"
    macro_content = (
        f"/detector/setGeometryFile config/{geometry}\n"
        f"/output/setFileName {run_dir}/{output}\n"
        f"/run/initialize\n"
        f"/vis/disable\n"
        f"/hits/setVerbose {verbose}\n"
        f"\n"
        f"/gun/particle {particle}\n"
        f"/gun/energy {energy} {energy_unit}\n"
        f"/gun/position {pos_x} {pos_y} {pos_z} {pos_unit}\n"
        f"/gun/direction {dir_x} {dir_y} {dir_z}\n"
        f"\n"
        f"/run/beamOn {n_events}\n"
    )
    macro_path.write_text(macro_content)

    # Save run metadata
    meta = {
        "started": stamp,
        "geometry": geometry,
        "particle": particle,
        "energy": f"{energy} {energy_unit}",
        "position": f"{pos_x} {pos_y} {pos_z} {pos_unit}",
        "direction": f"{dir_x} {dir_y} {dir_z}",
        "nEvents": int(n_events),
        "outputFile": output,
        "status": "running",
    }
    (run_dir / "meta.json").write_text(json.dumps(meta, indent=2))

    # Launch G4sim as a subprocess
    proc = await asyncio.create_subprocess_exec(
        str(G4SIM_BIN), str(macro_path),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.STDOUT,
        cwd=str(PROJECT_DIR),
    )
    current_process["proc"] = proc
    current_process["info"] = {
        "run_dir": str(run_dir),
        "meta": meta,
        "log_lines": [],
    }

    # Fire-and-forget: monitor the process
    asyncio.create_task(_monitor_process(proc, run_dir, meta))

    return {"status": "started", "runId": stamp}


async def _monitor_process(proc, run_dir: Path, meta: dict):
    """Read stdout until the process exits, then finalise metadata."""
    try:
        while True:
            line = await proc.stdout.readline()
            if not line:
                break
            text = line.decode(errors="replace").rstrip()
            if current_process["info"]:
                current_process["info"]["log_lines"].append(text)
    except Exception:
        pass

    await proc.wait()
    meta["status"] = "completed" if proc.returncode == 0 else f"error (code {proc.returncode})"
    meta["finished"] = datetime.now().strftime("%Y%m%d_%H%M%S")
    (run_dir / "meta.json").write_text(json.dumps(meta, indent=2))

    # Save full log
    if current_process["info"]:
        (run_dir / "log.txt").write_text("\n".join(current_process["info"]["log_lines"]))

    current_process["proc"] = None
    current_process["info"] = None


@app.post("/api/stop")
async def stop_run():
    """Kill the running simulation."""
    proc = current_process["proc"]
    if proc is None:
        return {"status": "no simulation running"}
    try:
        proc.kill()
    except ProcessLookupError:
        pass
    return {"status": "killed"}


@app.get("/api/status")
async def run_status():
    """Return current run status and recent log lines."""
    if current_process["proc"] is None:
        return {"running": False}
    info = current_process["info"]
    return {
        "running": True,
        "runDir": info["run_dir"],
        "meta": info["meta"],
        "logTail": info["log_lines"][-100:],
        "logLength": len(info["log_lines"]),
    }


# ---------------------------------------------------------------------------
#  WebSocket for live log streaming
# ---------------------------------------------------------------------------
@app.websocket("/ws/log")
async def websocket_log(ws: WebSocket):
    await ws.accept()
    sent = 0
    try:
        while True:
            if current_process["info"]:
                lines = current_process["info"]["log_lines"]
                if len(lines) > sent:
                    for line in lines[sent:]:
                        await ws.send_text(line)
                    sent = len(lines)
            else:
                # No process running — check if we should close
                if sent > 0:
                    await ws.send_text("__DONE__")
                    break
            await asyncio.sleep(0.2)
    except WebSocketDisconnect:
        pass


# ---------------------------------------------------------------------------
#  Run history
# ---------------------------------------------------------------------------
@app.get("/api/runs")
async def list_runs():
    """Return list of past runs with metadata."""
    runs = []
    for meta_file in sorted(RUNS_DIR.glob("*/meta.json"), reverse=True):
        try:
            meta = json.loads(meta_file.read_text())
            meta["runId"] = meta_file.parent.name
            runs.append(meta)
        except Exception:
            pass
    return runs


# ---------------------------------------------------------------------------
#  Results API
# ---------------------------------------------------------------------------
@app.get("/api/results/{run_id}/files")
async def result_files(run_id: str):
    """List output files for a run."""
    run_dir = RUNS_DIR / run_id
    if not run_dir.exists():
        return JSONResponse({"error": "Run not found"}, status_code=404)
    files = [f.name for f in run_dir.iterdir() if f.suffix in (".root", ".mac", ".json", ".txt")]
    return sorted(files)


@app.get("/api/results/{run_id}/download/{filename}")
async def download_file(run_id: str, filename: str):
    """Download a result file."""
    fpath = RUNS_DIR / run_id / filename
    if not fpath.exists():
        return JSONResponse({"error": "File not found"}, status_code=404)
    return FileResponse(fpath, filename=filename)


@app.get("/api/results/{run_id}/log")
async def get_log(run_id: str):
    """Return the log file for a completed run."""
    log_path = RUNS_DIR / run_id / "log.txt"
    if not log_path.exists():
        return JSONResponse({"error": "Log not found"}, status_code=404)
    return {"log": log_path.read_text()}


@app.get("/api/results/{run_id}/branches")
async def list_branches(run_id: str):
    """List TTree branches in the ROOT output."""
    run_dir = RUNS_DIR / run_id
    root_files = list(run_dir.glob("*.root"))
    if not root_files:
        return JSONResponse({"error": "No ROOT file found"}, status_code=404)
    f = uproot.open(root_files[0])
    tree = f["events"]
    branches = [k for k in tree.keys()]
    return {"file": root_files[0].name, "branches": branches}


@app.get("/api/results/{run_id}/plot/{branch}")
async def plot_branch(run_id: str, branch: str):
    """Return a Plotly JSON histogram for a given branch."""
    run_dir = RUNS_DIR / run_id
    root_files = list(run_dir.glob("*.root"))
    if not root_files:
        return JSONResponse({"error": "No ROOT file found"}, status_code=404)

    f = uproot.open(root_files[0])
    tree = f["events"]

    try:
        data = tree[branch].array(library="np")
    except Exception as e:
        return JSONResponse({"error": str(e)}, status_code=400)

    # Flatten if it's a jagged array
    import numpy as np
    if hasattr(data, 'tolist'):
        flat = []
        for item in data:
            if hasattr(item, '__iter__') and not isinstance(item, str):
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


@app.get("/api/results/{run_id}/plot3d")
async def plot_3d(run_id: str):
    """Return Plotly JSON for a 3D scatter of hit positions across all detectors."""
    run_dir = RUNS_DIR / run_id
    root_files = list(run_dir.glob("*.root"))
    if not root_files:
        return JSONResponse({"error": "No ROOT file found"}, status_code=404)

    import numpy as np
    f = uproot.open(root_files[0])
    tree = f["events"]
    keys = tree.keys()

    # Find detector prefixes by looking for *_x branches
    detectors = [k.replace("_x", "") for k in keys if k.endswith("_x")]

    fig = go.Figure()
    for det in detectors:
        try:
            x = tree[f"{det}_x"].array(library="np")
            y = tree[f"{det}_y"].array(library="np")
            z = tree[f"{det}_z"].array(library="np")
            # Flatten jagged arrays
            xf = np.concatenate([np.atleast_1d(a) for a in x]) if len(x) else np.array([])
            yf = np.concatenate([np.atleast_1d(a) for a in y]) if len(y) else np.array([])
            zf = np.concatenate([np.atleast_1d(a) for a in z]) if len(z) else np.array([])

            if len(xf) == 0:
                continue

            # Subsample if too many points
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
        title="Hit positions (3D)",
        scene=dict(xaxis_title="x [mm]", yaxis_title="y [mm]", zaxis_title="z [mm]"),
        template="plotly_white",
        height=600,
        margin=dict(l=0, r=0, t=40, b=0),
    )
    return {"plotJSON": fig.to_json()}
