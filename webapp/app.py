"""
G4sim Web Dashboard — FastAPI backend

A lightweight local web interface for configuring, running, and
inspecting Geant4 simulations.  Keeps the CLI workflow intact;
this is just a convenience layer on top of the same build/G4sim binary.
"""

import sys
from pathlib import Path

# Ensure the webapp directory is on sys.path so that sibling imports
# (config, routers.*, services.*) work regardless of the cwd.
_webapp_dir = str(Path(__file__).resolve().parent)
if _webapp_dir not in sys.path:
    sys.path.insert(0, _webapp_dir)

from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates

from config import WEBAPP_DIR
from routers.config_api import router as config_router
from routers.run_api import router as run_router
from routers.results_api import router as results_router
from routers.condor_api import router as condor_router

# ---------------------------------------------------------------------------
#  App setup
# ---------------------------------------------------------------------------
app = FastAPI(title="G4sim Dashboard")
app.mount("/static", StaticFiles(directory=WEBAPP_DIR / "static"), name="static")
templates = Jinja2Templates(directory=WEBAPP_DIR / "templates")

# Register routers
app.include_router(config_router)
app.include_router(run_router)
app.include_router(results_router)
app.include_router(condor_router)


# ---------------------------------------------------------------------------
#  Pages
# ---------------------------------------------------------------------------
@app.get("/", response_class=HTMLResponse)
async def index(request: Request):
    return templates.TemplateResponse("index.html", {"request": request})
