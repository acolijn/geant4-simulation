"""
Shared paths and constants for the G4sim web dashboard.
"""

from pathlib import Path

WEBAPP_DIR  = Path(__file__).resolve().parent
PROJECT_DIR = WEBAPP_DIR.parent
BUILD_DIR   = PROJECT_DIR / "build"
CONFIG_DIR  = PROJECT_DIR / "config"
RUNS_DIR    = WEBAPP_DIR / "runs"
G4SIM_BIN   = BUILD_DIR / "G4sim"

# Ensure the runs directory exists
RUNS_DIR.mkdir(exist_ok=True)
