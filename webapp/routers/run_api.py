"""
Run API — start / stop / status, live log streaming, and run history.

Uses the General Particle Source (GPS) for macro generation.
"""

import asyncio
import json
import re
from datetime import datetime

from fastapi import APIRouter, Request, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse

from config import CONFIG_DIR, PROJECT_DIR, RUNS_DIR, G4SIM_BIN

from services.simulation import current_process, start_simulation, stop_simulation, get_status

# Regex for values safe to interpolate into Geant4 macro commands.
# Allows word chars (letters, digits, underscore), hyphens, dots, plus/minus.
_SAFE_MACRO_VALUE = re.compile(r"^[\w.+\-]+$")

router = APIRouter(tags=["run"])


def _build_gps_macro(body: dict, run_dir, output: str) -> str:
    """Build a Geant4 macro string using GPS commands from the request body."""
    geometry    = body.get("geometry", "geometry.json")
    verbose     = body.get("verboseHits", "0")
    n_events    = body.get("nEvents", "10000")

    # ── Source settings ──
    particle    = body.get("particle", "gamma")
    ion_za      = body.get("ionZA", "")           # e.g. "6 12"  (Z A)
    ion_charge  = body.get("ionCharge", "0")
    ion_excite  = body.get("ionExcitation", "0")   # keV
    ene_type    = body.get("eneType", "Mono")
    energy      = body.get("energy", "1")
    energy_unit = body.get("energyUnit", "MeV")
    ene_min     = body.get("eneMin", "0")
    ene_max     = body.get("eneMax", "1000")
    ene_sigma   = body.get("eneSigma", "10")
    ene_alpha   = body.get("eneAlpha", "-1")
    ene_gradient  = body.get("eneGradient", "0")
    ene_intercept = body.get("eneIntercept", "1")

    pos_type    = body.get("posType", "Point")
    pos_shape   = body.get("posShape", "")
    pos_x       = body.get("posX", "0")
    pos_y       = body.get("posY", "0")
    pos_z       = body.get("posZ", "0")
    pos_unit    = body.get("posUnit", "cm")
    pos_radius  = body.get("posRadius", "1")
    pos_halfz   = body.get("posHalfz", "1")
    pos_halfx   = body.get("posHalfx", "1")
    pos_halfy   = body.get("posHalfy", "1")
    pos_halfz_box = body.get("posHalfzBox", "1")
    pos_confine = body.get("posConfine", "")

    summarize   = body.get("summarizeHits", "0")

    ang_type    = body.get("angType", "iso")
    ang_mintheta = body.get("angMintheta", "")
    ang_maxtheta = body.get("angMaxtheta", "")
    ang_minphi   = body.get("angMinphi", "")
    ang_maxphi   = body.get("angMaxphi", "")
    ang_fx       = body.get("angFx", "0")
    ang_fy       = body.get("angFy", "0")
    ang_fz       = body.get("angFz", "0")
    dir_x        = body.get("dirX", "1")
    dir_y        = body.get("dirY", "0")
    dir_z        = body.get("dirZ", "0")

    lines = []
    lines.append(f"/detector/setGeometryFile config/{geometry}")
    lines.append(f"/output/setFileName {run_dir}/{output}")

    lines.append("/run/initialize")
    lines.append("/vis/disable")
    lines.append(f"/hits/setVerbose {verbose}")
    lines.append("")
    lines.append("# --- General Particle Source ---")
    lines.append(f"/gps/particle {particle}")

    # Ion specification
    if particle == "ion" and ion_za:
        parts = ion_za.split()
        if len(parts) == 2:
            z_str, a_str = parts
            ion_cmd = f"/gps/ion {z_str} {a_str}"
            # Append charge / excitation only when non-zero
            q_val = int(ion_charge) if ion_charge else 0
            e_val = float(ion_excite) if ion_excite else 0.0
            if q_val != 0 or e_val != 0.0:
                ion_cmd += f" {q_val} {e_val}"
            lines.append(ion_cmd)

    # Energy — skip for ions (kinetic energy = 0, decay physics handles emission)
    if particle != "ion":
        lines.append(f"/gps/ene/type {ene_type}")
        if ene_type == "Mono":
            lines.append(f"/gps/ene/mono {energy} {energy_unit}")
        elif ene_type == "Lin":
            lines.append(f"/gps/ene/min {ene_min} {energy_unit}")
            lines.append(f"/gps/ene/max {ene_max} {energy_unit}")
            lines.append(f"/gps/ene/gradient {ene_gradient}")
            lines.append(f"/gps/ene/intercept {ene_intercept}")
        elif ene_type == "Pow":
            lines.append(f"/gps/ene/min {ene_min} {energy_unit}")
            lines.append(f"/gps/ene/max {ene_max} {energy_unit}")
            lines.append(f"/gps/ene/alpha {ene_alpha}")
        elif ene_type == "Gauss":
            lines.append(f"/gps/ene/mono {energy} {energy_unit}")
            lines.append(f"/gps/ene/sigma {ene_sigma} {energy_unit}")
    else:
        lines.append("/gps/ene/type Mono")
        lines.append("/gps/ene/mono 0 eV")

    lines.append("")

    # Position / shape
    lines.append(f"/gps/pos/type {pos_type}")
    if pos_type in ("Volume", "Surface") and pos_shape:
        lines.append(f"/gps/pos/shape {pos_shape}")
    lines.append(f"/gps/pos/centre {pos_x} {pos_y} {pos_z} {pos_unit}")

    if pos_type in ("Volume", "Surface") and pos_shape:
        if pos_shape in ("Cylinder", "Sphere", "Circle", "Ellipsoid"):
            lines.append(f"/gps/pos/radius {pos_radius} {pos_unit}")
        if pos_shape == "Cylinder":
            lines.append(f"/gps/pos/halfz {pos_halfz} {pos_unit}")
        if pos_shape in ("Box", "Para"):
            lines.append(f"/gps/pos/halfx {pos_halfx} {pos_unit}")
            lines.append(f"/gps/pos/halfy {pos_halfy} {pos_unit}")
            lines.append(f"/gps/pos/halfz {pos_halfz_box} {pos_unit}")

    if pos_confine and pos_type == "Volume":
        lines.append(f"/gps/pos/confine {pos_confine}")
    lines.append("")

    # Angular distribution (skip for ions — decay is isotropic by physics)
    if particle != "ion":
        lines.append(f"/gps/ang/type {ang_type}")
        if ang_type in ("iso", "cos"):
            if ang_mintheta:
                lines.append(f"/gps/ang/mintheta {ang_mintheta} deg")
            if ang_maxtheta:
                lines.append(f"/gps/ang/maxtheta {ang_maxtheta} deg")
            if ang_minphi:
                lines.append(f"/gps/ang/minphi {ang_minphi} deg")
            if ang_maxphi:
                lines.append(f"/gps/ang/maxphi {ang_maxphi} deg")
        elif ang_type == "focused":
            lines.append(f"/gps/ang/focuspoint {ang_fx} {ang_fy} {ang_fz} {pos_unit}")
        elif ang_type in ("beam1d", "beam2d"):
            lines.append(f"/gps/direction {dir_x} {dir_y} {dir_z}")

    lines.append("")
    # Hit summarisation mode (sum energy & average position per volume)
    if summarize == "1":
        lines.append("/output/setSummarize 1")
        lines.append("")
    lines.append(f"/run/beamOn {n_events}")
    lines.append("")

    return "\n".join(lines)


@router.post("/api/run")
async def start_run(request: Request):
    """Start a simulation run.  Expects JSON body with GPS parameters."""
    if current_process["proc"] is not None:
        return JSONResponse({"error": "A simulation is already running"}, status_code=409)

    body = await request.json()
    geometry    = body.get("geometry", "geometry.json")
    n_events    = body.get("nEvents", "10000")
    output      = body.get("outputFile", "G4sim.root")

    # ── Input sanitisation ────────────────────────────────
    # Collect all fields that will appear in the macro.
    # Fields that need special validation are handled separately.
    # Validate geometry and outputFile (these are interpolated into the macro)
    for fld_name, fld_val in (("geometry", geometry), ("outputFile", output)):
        if not _SAFE_MACRO_VALUE.match(fld_val):
            return JSONResponse(
                {"error": f"Invalid characters in '{fld_name}'"},
                status_code=400,
            )

    _SKIP_FIELDS = {"geometry", "outputFile", "ionZA", "ionCharge", "ionExcitation"}
    macro_fields = {}
    for key, val in body.items():
        if key in _SKIP_FIELDS:
            continue
        macro_fields[key] = str(val)

    for field_name, value in macro_fields.items():
        if value and not _SAFE_MACRO_VALUE.match(value):
            return JSONResponse(
                {"error": f"Invalid characters in '{field_name}'"},
                status_code=400,
            )

    # Validate ion fields only when particle == ion
    if body.get("particle") == "ion":
        ion_za = str(body.get("ionZA", ""))
        if ion_za and not re.match(r"^\d+\s+\d+$", ion_za):
            return JSONResponse({"error": "Invalid ion Z A specification"}, status_code=400)
        for fld in ("ionCharge", "ionExcitation"):
            v = str(body.get(fld, "0"))
            if v and not _SAFE_MACRO_VALUE.match(v):
                return JSONResponse({"error": f"Invalid characters in '{fld}'"}, status_code=400)

    # Validate geometry filename stays inside config/
    geom_path = (CONFIG_DIR / geometry).resolve()
    if not str(geom_path).startswith(str(CONFIG_DIR.resolve())):
        return JSONResponse({"error": "Invalid geometry filename"}, status_code=400)

    # Create a unique run directory
    stamp   = datetime.now().strftime("%Y%m%d_%H%M%S")
    run_dir = RUNS_DIR / stamp
    run_dir.mkdir(parents=True, exist_ok=True)

    # Generate the GPS macro
    macro_path = run_dir / "run.mac"
    macro_content = _build_gps_macro(body, run_dir, output)
    macro_path.write_text(macro_content)

    # Save run metadata
    is_ion = body.get("particle", "gamma") == "ion"
    meta = {
        "started": stamp,
        "geometry": geometry,
        "particle": (body.get("ionName", "") or "ion") if is_ion else body.get("particle", "gamma"),
        "ionZA": body.get("ionZA", ""),
        "energy": "0 eV (decay)" if is_ion else f"{body.get('energy', '1')} {body.get('energyUnit', 'MeV')}",
        "sourceType": body.get("posType", "Point"),
        "angularType": "— (decay)" if is_ion else body.get("angType", "iso"),
        "summarizeHits": body.get("summarizeHits", "0"),
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
