"""
Condor API — submit / status / cancel HTCondor batch jobs.
"""

import json
from datetime import datetime

from fastapi import APIRouter, Request
from fastapi.responses import JSONResponse

from config import RUNS_DIR
from routers.run_api import build_gps_macro, validate_run_body
from services.condor import (
    cancel_condor_jobs,
    check_condor_available,
    get_condor_status,
    get_full_queue,
    merge_output,
    submit_condor_jobs,
)

router = APIRouter(tags=["condor"])


@router.get("/api/condor/available")
async def condor_available():
    """Check if HTCondor is available on this machine."""
    ok = await check_condor_available()
    return {"available": ok}


@router.get("/api/condor/queue")
async def condor_queue():
    """Return all user's jobs currently in the Condor queue."""
    jobs = await get_full_queue()
    # Summarise
    counts = {}
    for j in jobs:
        s = j["status"]
        counts[s] = counts.get(s, 0) + 1
    return {"jobs": jobs, "total": len(jobs), "counts": counts}


@router.post("/api/condor/submit")
async def condor_submit(request: Request):
    """Submit a batch of Condor jobs."""
    body = await request.json()

    # Validate fields (same rules as local run)
    err = validate_run_body(body)
    if err:
        return JSONResponse({"error": err}, status_code=400)

    n_jobs = int(body.get("nCondorJobs", 10))
    if n_jobs < 1 or n_jobs > 10000:
        return JSONResponse({"error": "nCondorJobs must be 1–10 000"}, status_code=400)

    geometry = body.get("geometry", "geometry.json")
    output = body.get("outputFile", "G4sim.root")
    n_events = body.get("nEvents", "10000")

    # Create a unique run directory
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    run_dir = RUNS_DIR / stamp
    run_dir.mkdir(parents=True, exist_ok=True)

    # Build run metadata
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
        "n_jobs": n_jobs,
        "runMode": "condor",
        "status": "submitting",
    }
    (run_dir / "meta.json").write_text(json.dumps(meta, indent=2))

    result = await submit_condor_jobs(
        run_dir=run_dir,
        macro_builder=build_gps_macro,
        body=body,
        n_jobs=n_jobs,
        meta=meta,
    )

    if "error" in result:
        return JSONResponse({"error": result["error"]}, status_code=500)

    return {
        "status": "submitted",
        "runId": stamp,
        "cluster_id": result["cluster_id"],
        "n_jobs": result["n_jobs"],
    }


@router.get("/api/condor/status/{run_id}")
async def condor_status(run_id: str):
    """Get per-job Condor status for a run."""
    return await get_condor_status(run_id)


@router.post("/api/condor/cancel/{run_id}")
async def condor_cancel(run_id: str):
    """Cancel all Condor jobs for a run."""
    return await cancel_condor_jobs(run_id)


@router.post("/api/condor/cancel-cluster/{cluster_id}")
async def condor_cancel_cluster(cluster_id: str):
    """Cancel all jobs in a Condor cluster by cluster ID (direct condor_rm)."""
    import asyncio
    try:
        proc = await asyncio.create_subprocess_exec(
            "condor_rm", cluster_id,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await proc.communicate()
        if proc.returncode != 0:
            return {"error": stderr.decode().strip() or "condor_rm failed"}
        return {"status": "cancelled", "cluster_id": cluster_id}
    except FileNotFoundError:
        return {"error": "condor_rm not found"}
    except Exception as e:
        return {"error": str(e)}


@router.post("/api/condor/merge/{run_id}")
async def condor_merge(run_id: str):
    """Manually merge per-job ROOT output files."""
    return await merge_output(run_id)
