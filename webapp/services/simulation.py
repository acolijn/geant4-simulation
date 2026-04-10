"""
Simulation process management — start, monitor, and kill G4sim runs.
"""

import asyncio
import json
from datetime import datetime
from pathlib import Path

from config import G4SIM_BIN, PROJECT_DIR, RUNS_DIR


# In-memory state for the currently running simulation
current_process: dict = {"proc": None, "info": None}


async def start_simulation(run_dir: Path, macro_path: Path, meta: dict) -> None:
    """Launch G4sim as a subprocess and start monitoring it."""
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
    asyncio.create_task(_monitor_process(proc, run_dir, meta))


LOG_BUFFER_MAX = 50_000  # keep last N lines in memory


async def _monitor_process(proc, run_dir: Path, meta: dict) -> None:
    """Read stdout until the process exits, then finalise metadata."""
    log_file = run_dir / "log" / "log.txt"
    log_file.parent.mkdir(exist_ok=True)
    try:
        with open(log_file, "w") as flog:
            while True:
                data = await proc.stdout.read(8192)
                if not data:
                    break
                for text in data.decode(errors="replace").splitlines():
                    flog.write(text + "\n")
                    if current_process["info"]:
                        buf = current_process["info"]["log_lines"]
                        buf.append(text)
                        if len(buf) > LOG_BUFFER_MAX:
                            del buf[: len(buf) - LOG_BUFFER_MAX]
    except Exception:
        pass

    await proc.wait()
    meta["status"] = (
        "completed" if proc.returncode == 0 else f"error (code {proc.returncode})"
    )
    meta["finished"] = datetime.now().strftime("%Y%m%d_%H%M%S")
    (run_dir / "meta.json").write_text(json.dumps(meta, indent=2))

    current_process["proc"] = None
    current_process["info"] = None


async def stop_simulation() -> str:
    """Kill the running simulation.  Returns a status string."""
    proc = current_process["proc"]
    if proc is None:
        return "no simulation running"
    try:
        proc.kill()
    except ProcessLookupError:
        pass
    return "killed"


def get_status() -> dict:
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
