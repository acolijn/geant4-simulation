"""
Run API — start / stop / status, live log streaming, and run history.
"""

import asyncio
import json
from datetime import datetime

from fastapi import APIRouter, Request, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse

from config import CONFIG_DIR, PROJECT_DIR, RUNS_DIR, G4SIM_BIN
from services.simulation import current_process, start_simulation, stop_simulation, get_status

router = APIRouter(tags=["run"])


@router.post("/api/run")
async def start_run(request: Request):
    """Start a simulation run.  Expects JSON body with run parameters."""
    if current_process["proc"] is not None:
        return JSONResponse({"error": "A simulation is already running"}, status_code=409)

    body = await request.json()
    geometry    = body.get("geometry", "geometry.json")
    particle    = body.get("particle", "gamma")
    energy      = body.get("energy", "1")
    energy_unit = body.get("energyUnit", "MeV")
    pos_x       = body.get("posX", "-10")
    pos_y       = body.get("posY", "0")
    pos_z       = body.get("posZ", "0")
    pos_unit    = body.get("posUnit", "cm")
    dir_x       = body.get("dirX", "1")
    dir_y       = body.get("dirY", "0")
    dir_z       = body.get("dirZ", "0")
    n_events    = body.get("nEvents", "10000")
    output      = body.get("outputFile", "G4sim.root")
    verbose     = body.get("verboseHits", "0")

    # Create a unique run directory
    stamp   = datetime.now().strftime("%Y%m%d_%H%M%S")
    run_dir = RUNS_DIR / stamp
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

    # Launch the simulation
    await start_simulation(run_dir, macro_path, meta)

    return {"status": "started", "runId": stamp}


@router.post("/api/stop")
async def stop_run():
    """Kill the running simulation."""
    result = await stop_simulation()
    return {"status": result}


@router.get("/api/status")
async def run_status():
    """Return current run status and recent log lines."""
    return get_status()


# ---------------------------------------------------------------------------
#  WebSocket for live log streaming
# ---------------------------------------------------------------------------
@router.websocket("/ws/log")
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
@router.get("/api/runs")
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
