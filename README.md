# Geant4 Simulation — Neutron Transport

A Geant4 application for simulating neutron transport using high-precision neutron physics (`FTFP_BERT_HP`). The detector geometry is defined via JSON configuration files, making it easy to modify without recompiling.

[![Documentation Status](https://readthedocs.org/projects/geant4-simulation/badge/?version=latest)](https://geant4-simulation.readthedocs.io/en/latest/?badge=latest)

---

## Table of Contents

- [Prerequisites](#prerequisites)
- [Installation](#installation)
  - [1. Install Conda](#1-install-conda)
  - [2. Create the Environment](#2-create-the-environment)
  - [3. Activate the Environment](#3-activate-the-environment)
  - [4. Build the Project](#4-build-the-project)
- [Running the Simulation](#running-the-simulation)
  - [Interactive Mode (with Visualization)](#interactive-mode-with-visualization)
  - [Batch Mode (no Visualization)](#batch-mode-no-visualization)
- [Project Structure](#project-structure)
- [Web Dashboard](#web-dashboard)
- [Configuration](#configuration)
  - [Geometry Files](#geometry-files)
  - [Macro Commands](#macro-commands)
  - [Particle Gun](#particle-gun)
- [Output](#output)
- [Troubleshooting](#troubleshooting)
- [Documentation](#documentation)

---

## Prerequisites

| Dependency | Version | Notes |
|---|---|---|
| **Conda** | Miniconda or Anaconda | Package manager for all other dependencies |
| **Geant4** | 11.x | Includes HP neutron data files |
| **ROOT** | 6.x | For output histograms and trees |
| **CMake** | 3.16+ | Build system |
| **nlohmann_json** | — | JSON parsing library |

> All C++ dependencies (Geant4, ROOT, CMake, nlohmann_json) are installed automatically via the provided Conda environment file. You only need to have Conda itself installed.

---

## Installation

### 1. Install Conda

If you don't already have Conda, install **Miniconda** (lightweight) or **Anaconda**:

- **Miniconda** (recommended): <https://docs.conda.io/en/latest/miniconda.html>
- **Anaconda**: <https://www.anaconda.com/download>

Verify the installation:

```bash
conda --version
```

### 2. Create the Environment

Clone the repository and create the Conda environment from the included `environment.yml`:

```bash
git clone https://github.com/acolijn/geant4-simulation.git
cd geant4-simulation

conda env create -f environment.yml
```

This creates a Conda environment named **`g4`** with Geant4, ROOT, CMake, and nlohmann_json.

> **Note:** The initial environment creation can take several minutes as it downloads and installs Geant4 and ROOT.

### 3. Activate the Environment

```bash
conda activate g4
```

You need to activate this environment every time you open a new terminal session.

### 4. Build the Project

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

On macOS, if `nproc` is not available, use:

```bash
make -j$(sysctl -n hw.ncpu)
```

After a successful build the executable is located at `build/G4sim`.

---

## Running the Simulation

All commands below assume you are in the **project root directory** (not inside `build/`).

### Interactive Mode (with Visualization)

Launch without arguments to open the Qt-based visualization GUI:

```bash
build/G4sim
```

This executes the default macro `macros/vis.mac`, which sets up the geometry, visualization, and particle gun. You can then interact with the detector in the 3D viewer and fire events from the GUI.

### Batch Mode (no Visualization)

Run with a macro file to execute in batch mode:

```bash
build/G4sim macros/batch.mac
```

The `batch.mac` macro disables visualization and runs 100 000 events by default. Edit the macro to adjust the number of events (`/run/beamOn`), particle type, energy, or position.

---

## Project Structure

```
geant4-simulation/
├── G4sim.cc                   # Main application entry point
├── CMakeLists.txt             # CMake build configuration
├── environment.yml            # Conda environment specification
├── include/                   # C++ header files
│   ├── DetectorConstruction.hh
│   ├── ActionInitialization.hh
│   ├── EventAction.hh
│   ├── RunAction.hh
│   ├── PrimaryGeneratorAction.hh
│   ├── GeometryParser.hh
│   ├── MySensitiveDetector.hh
│   ├── MyHit.hh
│   └── json.hpp
├── src/                       # C++ source files
│   ├── DetectorConstruction.cc
│   ├── ActionInitialization.cc
│   ├── EventAction.cc
│   ├── RunAction.cc
│   ├── PrimaryGeneratorAction.cc
│   ├── GeometryParser.cc
│   ├── MySensitiveDetector.cc
│   └── MyHit.cc
├── macros/                    # Geant4 macro files
│   ├── vis.mac                # Interactive mode with visualization
│   └── batch.mac              # Batch mode (no visualization)
├── config/                    # Detector geometry (JSON)
│   ├── geometry.json
│   ├── geometry-4.json
│   ├── geometry_all.json
│   └── materials.json
└── webapp/                    # Local web dashboard (FastAPI)
    ├── app.py                 # Backend server
    ├── requirements.txt       # Python dependencies
    ├── templates/             # HTML pages
    ├── static/                # CSS & JavaScript
    └── runs/                  # Auto-created output per run (gitignored)
```

---

## Web Dashboard

A local web interface for configuring, running, and inspecting simulations — no need to edit `.mac` files by hand.

### Starting the Dashboard

```bash
conda activate g4
cd webapp
uvicorn app:app --host 127.0.0.1 --port 8000
```

Then open **http://127.0.0.1:8000** in your browser.

### Tabs

| Tab | Description |
|---|---|
| **Config** | Select geometry file (or upload a new JSON), set particle type, energy, position, direction, number of events, and output file name |
| **Run** | Start / stop simulation, live progress bar, streaming log, and run history |
| **Results** | Browse completed runs, download ROOT / log files, plot histograms, or view a 3D hit map |

Each run is saved in `webapp/runs/<timestamp>/` with the generated macro, metadata, ROOT output, and log.

> **Note:** The dashboard is for local use only. It spawns `G4sim` as a subprocess on your machine.

The command-line workflow (`build/G4sim macros/batch.mac`) continues to work exactly as before.

---

## Configuration

### Geometry Files

The detector geometry is defined in JSON files under `config/`. These files describe volumes, materials, positions, and rotations — and can be created or edited with the companion [Geant4 Geometry Editor](https://github.com/acolijn/geant4-geometry-editor).

### Macro Commands

Select the geometry file in your Geant4 macro with:

```
/detector/setGeometryFile config/geometry.json
```

Optionally set the ROOT output file name (default is `G4sim.root`):

```
/output/setFileName myrun.root
```

Both commands must appear **before** `/run/initialize`.

### Particle Gun

The default primary particle is a neutron at 1 MeV. Override it in your macro:

```
/gun/particle gamma
/gun/energy 1 MeV
/gun/position -10 0 0 cm
/gun/direction 1 0 0
```

---

## Output

The simulation writes a ROOT file (default **`G4sim.root`**) in the current working directory, containing a TTree named **`events`**. The file name can be changed with `/output/setFileName` in your macro.

Branches are created dynamically for each sensitive detector defined in the geometry. For a detector named `<det>`, the following branches are created:

| Branch | Type | Description |
|---|---|---|
| `<det>_nHits` | `Int_t` | Number of hits in this detector |
| `<det>_x` | `vector<double>` | Hit x-positions (mm) |
| `<det>_y` | `vector<double>` | Hit y-positions (mm) |
| `<det>_z` | `vector<double>` | Hit z-positions (mm) |
| `<det>_E` | `vector<double>` | Energy deposits per hit (MeV) |
| `<det>_volName` | `vector<string>` | Volume name for each hit |

You can inspect the output with ROOT:

```bash
root -l G4sim.root
root [0] events->Print()
root [1] events->Draw("<det>_E")
```

---

## Troubleshooting

| Problem | Solution |
|---|---|
| `conda: command not found` | Install Miniconda/Anaconda and restart your terminal |
| `conda env create` is very slow | Try `conda config --set solver libmamba` for the faster solver |
| `cmake` can't find Geant4 or ROOT | Make sure the `g4` environment is activated: `conda activate g4` |
| `nproc: command not found` (macOS) | Use `make -j$(sysctl -n hw.ncpu)` instead |
| Segfault on exit | This is a known Geant4 11.x issue with HP physics static destructors; the application uses `std::quick_exit(0)` to work around it |
| Visualization window doesn't open | Ensure Qt is available; try running from a graphical terminal (not SSH without X-forwarding) |

---

## Documentation

Full documentation is available on [Read the Docs](https://geant4-simulation.readthedocs.io/en/latest/).
