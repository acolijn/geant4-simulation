"""
HTCondor job submission and management for G4sim batch runs.

Handles:
  - Generating per-job macros with distinct random seeds & output files
  - Writing a Condor submit description (.sub) file
  - Submitting, querying, and cancelling Condor jobs
  - Merging per-job ROOT output files with hadd
"""

import asyncio
import json
import logging
import re
import shutil
from datetime import datetime
from pathlib import Path
from typing import Optional

from config import BUILD_DIR, G4SIM_BIN, PROJECT_DIR, RUNS_DIR

logger = logging.getLogger("condor")

# ── In-memory tracking of Condor clusters ──────────────────
# runId  →  { cluster_id, n_jobs, run_dir, meta, ... }
condor_runs: dict[str, dict] = {}


# ---------------------------------------------------------------------------
#  Submit
# ---------------------------------------------------------------------------

async def submit_condor_jobs(
    run_dir: Path,
    macro_builder,            # callable(body, run_dir, output_file) → str
    body: dict,
    n_jobs: int,
    meta: dict,
) -> dict:
    """
    Generate per-job macros, write a .sub file, and call condor_submit.

    Returns {"cluster_id": ..., "n_jobs": ...} on success, or
    {"error": ...} on failure.
    """
    run_dir.mkdir(parents=True, exist_ok=True)

    base_output = body.get("outputFile", "G4sim.root").replace(".root", "")

    # ── Per-job macro files ────────────────────────────────
    for j in range(n_jobs):
        job_output = f"{base_output}_{j:03d}.root"
        macro_text = macro_builder(body, run_dir, job_output)
        # Insert a unique random seed right after /run/initialize
        seed = _make_seed(j)
        macro_text = _inject_seed(macro_text, seed)
        (run_dir / f"run_{j:03d}.mac").write_text(macro_text)

    # ── Condor submit description ──────────────────────────
    sub_text = _build_submit_file(run_dir, n_jobs)
    sub_path = run_dir / "condor.sub"
    sub_path.write_text(sub_text)

    # ── condor_submit ──────────────────────────────────────
    try:
        proc = await asyncio.create_subprocess_exec(
            "condor_submit", str(sub_path),
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            cwd=str(run_dir),
        )
        stdout, stderr = await proc.communicate()
    except FileNotFoundError:
        err = "condor_submit not found — is HTCondor installed and on PATH?"
        logger.error(err)
        meta["status"] = "submit_error"
        meta["condor_error"] = err
        (run_dir / "meta.json").write_text(json.dumps(meta, indent=2))
        return {"error": err}
    except Exception as exc:
        err = f"Failed to run condor_submit: {exc}"
        logger.error(err)
        meta["status"] = "submit_error"
        meta["condor_error"] = err
        (run_dir / "meta.json").write_text(json.dumps(meta, indent=2))
        return {"error": err}

    if proc.returncode != 0:
        err = stderr.decode(errors="replace").strip()
        logger.error("condor_submit failed: %s", err)
        meta["status"] = f"submit_error"
        meta["condor_error"] = err
        (run_dir / "meta.json").write_text(json.dumps(meta, indent=2))
        return {"error": err}

    cluster_id = _parse_cluster_id(stdout.decode())
    if cluster_id is None:
        err = "Could not parse cluster ID from condor_submit output"
        logger.error(err)
        meta["status"] = "submit_error"
        (run_dir / "meta.json").write_text(json.dumps(meta, indent=2))
        return {"error": err}

    # Update metadata
    meta["status"] = "condor_submitted"
    meta["cluster_id"] = cluster_id
    meta["n_jobs"] = n_jobs
    (run_dir / "meta.json").write_text(json.dumps(meta, indent=2))

    run_id = run_dir.name
    condor_runs[run_id] = {
        "cluster_id": cluster_id,
        "n_jobs": n_jobs,
        "run_dir": str(run_dir),
        "meta": meta,
    }

    # Start background polling
    asyncio.create_task(_poll_until_done(run_id))

    return {"cluster_id": cluster_id, "n_jobs": n_jobs}


# ---------------------------------------------------------------------------
#  Status
# ---------------------------------------------------------------------------

async def get_condor_status(run_id: str) -> dict:
    """
    Return per-job status for a condor run.

    Queries condor_q for active jobs and checks output files for
    completed ones.
    """
    info = condor_runs.get(run_id)
    if info is None:
        # Try to reconstruct from meta.json on disk
        meta_path = RUNS_DIR / run_id / "meta.json"
        if meta_path.exists():
            meta = json.loads(meta_path.read_text())
            if meta.get("cluster_id"):
                return {
                    "run_id": run_id,
                    "cluster_id": meta["cluster_id"],
                    "n_jobs": meta.get("n_jobs", 0),
                    "status": meta.get("status", "unknown"),
                    "jobs": [],
                    "merged": meta.get("merged", False),
                }
        return {"error": "Unknown run"}

    cluster_id = info["cluster_id"]
    n_jobs = info["n_jobs"]
    run_dir = Path(info["run_dir"])
    meta = info["meta"]

    # Query condor_q for this cluster
    jobs = await _query_condor_q(cluster_id)

    # Build per-job summary
    base_output = meta.get("outputFile", "G4sim.root").replace(".root", "")
    job_list = []
    n_done = 0
    n_running = 0
    n_idle = 0
    n_held = 0
    for j in range(n_jobs):
        job_id = f"{cluster_id}.{j}"
        cq = jobs.get(job_id, {})
        status_code = cq.get("JobStatus", None)
        status_str = _job_status_str(status_code)
        # If not in queue, check if output exists → completed
        output_file = run_dir / f"{base_output}_{j:03d}.root"
        if status_code is None and output_file.exists():
            status_str = "completed"
        elif status_code is None:
            status_str = "completed" if meta.get("status") in ("condor_done", "merged") else "unknown"

        if status_str == "completed":
            n_done += 1
        elif status_str == "running":
            n_running += 1
        elif status_str == "idle":
            n_idle += 1
        elif status_str == "held":
            n_held += 1

        job_list.append({
            "job_id": job_id,
            "process": j,
            "status": status_str,
            "output_exists": output_file.exists(),
        })

    overall = meta.get("status", "condor_submitted")

    return {
        "run_id": run_id,
        "cluster_id": cluster_id,
        "n_jobs": n_jobs,
        "status": overall,
        "merged": meta.get("merged", False),
        "counts": {
            "done": n_done,
            "running": n_running,
            "idle": n_idle,
            "held": n_held,
        },
        "jobs": job_list,
    }


# ---------------------------------------------------------------------------
#  Cancel
# ---------------------------------------------------------------------------

async def cancel_condor_jobs(run_id: str) -> dict:
    """Cancel all jobs in a Condor cluster via condor_rm."""
    info = condor_runs.get(run_id)
    if info is None:
        return {"error": "Unknown run"}

    cluster_id = info["cluster_id"]
    try:
        proc = await asyncio.create_subprocess_exec(
            "condor_rm", str(cluster_id),
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await proc.communicate()
    except FileNotFoundError:
        return {"error": "condor_rm not found — is HTCondor installed?"}

    run_dir = Path(info["run_dir"])
    meta = info["meta"]
    meta["status"] = "cancelled"
    (run_dir / "meta.json").write_text(json.dumps(meta, indent=2))

    return {"status": "cancelled", "cluster_id": cluster_id}


# ---------------------------------------------------------------------------
#  Merge
# ---------------------------------------------------------------------------

async def merge_output(run_id: str) -> dict:
    """Merge per-job ROOT files using hadd."""
    info = condor_runs.get(run_id)
    if info is None:
        # Try loading from disk
        meta_path = RUNS_DIR / run_id / "meta.json"
        if not meta_path.exists():
            return {"error": "Unknown run"}
        meta = json.loads(meta_path.read_text())
        run_dir = RUNS_DIR / run_id
        n_jobs = meta.get("n_jobs", 0)
        base_output = meta.get("outputFile", "G4sim.root").replace(".root", "")
    else:
        run_dir = Path(info["run_dir"])
        meta = info["meta"]
        n_jobs = info["n_jobs"]
        base_output = meta.get("outputFile", "G4sim.root").replace(".root", "")

    # Collect existing per-job ROOT files
    parts = []
    for j in range(n_jobs):
        f = run_dir / f"{base_output}_{j:03d}.root"
        if f.exists():
            parts.append(str(f))

    if not parts:
        return {"error": "No output files to merge"}

    merged_path = run_dir / meta.get("outputFile", "G4sim.root")

    # Find hadd (could be in ROOT's bin or on PATH)
    hadd = shutil.which("hadd")
    if hadd is None:
        return {"error": "hadd not found on PATH"}

    proc = await asyncio.create_subprocess_exec(
        hadd, "-f", str(merged_path), *parts,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        cwd=str(run_dir),
    )
    stdout, stderr = await proc.communicate()

    if proc.returncode != 0:
        err = stderr.decode(errors="replace").strip()
        return {"error": f"hadd failed: {err}"}

    meta["status"] = "merged"
    meta["merged"] = True
    meta["finished"] = datetime.now().strftime("%Y%m%d_%H%M%S")
    (run_dir / "meta.json").write_text(json.dumps(meta, indent=2))

    return {"status": "merged", "output": str(merged_path)}


# ---------------------------------------------------------------------------
#  Internal helpers
# ---------------------------------------------------------------------------

def _make_seed(job_index: int) -> int:
    """Generate a deterministic-but-unique seed per job."""
    import time
    # Combine current timestamp (seconds) with job index
    base = int(datetime.now().timestamp()) & 0x7FFFFFFF
    return (base + job_index * 7919) & 0x7FFFFFFF     # prime spacing


def _inject_seed(macro_text: str, seed: int) -> str:
    """Insert /random/setSeeds after /run/initialize."""
    seed2 = (seed * 31 + 17) & 0x7FFFFFFF
    seed_cmd = f"/random/setSeeds {seed} {seed2}"
    return macro_text.replace(
        "/run/initialize",
        f"/run/initialize\n{seed_cmd}",
        1,
    )


def _build_submit_file(run_dir: Path, n_jobs: int) -> str:
    """Generate a Condor submit description."""
    return (
        f"universe   = vanilla\n"
        f"executable = {G4SIM_BIN}\n"
        f"arguments  = {run_dir}/run_$INT(Process,%03d).mac\n"
        f"initialdir = {PROJECT_DIR}\n"
        f"getenv     = True\n"
        f"\n"
        f"output = {run_dir}/condor_$INT(Process,%03d).out\n"
        f"error  = {run_dir}/condor_$INT(Process,%03d).err\n"
        f"log    = {run_dir}/condor.log\n"
        f"\n"
        f"should_transfer_files = NO\n"
        f"\n"
        f"queue {n_jobs}\n"
    )


def _parse_cluster_id(output: str) -> Optional[int]:
    """
    Extract the cluster ID from condor_submit stdout.
    Typical line: '1 job(s) submitted to cluster 12345.'
    """
    m = re.search(r"submitted to cluster (\d+)", output)
    return int(m.group(1)) if m else None


_CONDOR_STATUS_MAP = {
    1: "idle",
    2: "running",
    3: "removed",
    4: "completed",
    5: "held",
    6: "transferring",
    7: "suspended",
}

def _job_status_str(code) -> str:
    if code is None:
        return "unknown"
    return _CONDOR_STATUS_MAP.get(int(code), f"status_{code}")


async def _query_condor_q(cluster_id: int) -> dict:
    """
    Run condor_q -json for a given cluster and return
    {job_id: {JobStatus: ..., ...}} dict.
    """
    try:
        proc = await asyncio.create_subprocess_exec(
            "condor_q", str(cluster_id), "-json",
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await proc.communicate()
        if proc.returncode != 0:
            return {}
        text = stdout.decode().strip()
        if not text or text == "[]":
            return {}
        items = json.loads(text)
        result = {}
        for item in items:
            cid = item.get("ClusterId", "")
            pid = item.get("ProcId", "")
            result[f"{cid}.{pid}"] = item
        return result
    except Exception as e:
        logger.warning("condor_q failed: %s", e)
        return {}


async def _poll_until_done(run_id: str, interval: float = 10.0) -> None:
    """
    Background task: poll condor_q until all jobs finish,
    then auto-merge output.
    """
    info = condor_runs.get(run_id)
    if info is None:
        return

    cluster_id = info["cluster_id"]
    n_jobs = info["n_jobs"]
    run_dir = Path(info["run_dir"])
    meta = info["meta"]

    while True:
        await asyncio.sleep(interval)

        # Check if run was cancelled
        if meta.get("status") == "cancelled":
            return

        jobs = await _query_condor_q(cluster_id)

        # If no jobs in the queue, they're all done
        if not jobs:
            # Verify all output files are present
            base_output = meta.get("outputFile", "G4sim.root").replace(".root", "")
            all_done = all(
                (run_dir / f"{base_output}_{j:03d}.root").exists()
                for j in range(n_jobs)
            )
            if all_done:
                meta["status"] = "condor_done"
                (run_dir / "meta.json").write_text(json.dumps(meta, indent=2))
                # Auto-merge
                result = await merge_output(run_id)
                logger.info("Auto-merge for %s: %s", run_id, result)
            else:
                meta["status"] = "condor_done"
                meta["merge_note"] = "Some output files missing — manual merge needed"
                (run_dir / "meta.json").write_text(json.dumps(meta, indent=2))
            return

        # Check for held jobs
        held = [jid for jid, j in jobs.items() if j.get("JobStatus") == 5]
        if held:
            meta["status"] = "condor_held"
            (run_dir / "meta.json").write_text(json.dumps(meta, indent=2))

        # Update running count
        n_running = sum(1 for j in jobs.values() if j.get("JobStatus") == 2)
        n_idle = sum(1 for j in jobs.values() if j.get("JobStatus") == 1)
        if n_running > 0:
            meta["status"] = "condor_running"
        elif n_idle > 0 and not held:
            meta["status"] = "condor_submitted"
        (run_dir / "meta.json").write_text(json.dumps(meta, indent=2))
