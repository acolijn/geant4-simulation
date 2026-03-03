# Geant4 Simulation — Particle Transport

A Geant4 application for simulating particle transport using high-precision physics (`FTFP_BERT_HP`). The detector geometry is defined via JSON configuration files, making it easy to modify without recompiling. A local web dashboard is included for configuring, launching, and analysing simulation runs.

Key features:

- **JSON-based geometry** — box, cylinder, sphere, ellipsoid, torus, trapezoid, polycone, assemblies, and boolean (union/subtraction) solids
- **Web dashboard** — configure GPS, launch runs, view live progress, plot histograms and 3D hit maps
- **True CSG rendering** — boolean solids displayed as real union/subtraction meshes (via `trimesh` + `manifold3d`)
- **Sensitive detectors** — per-volume hit collections with automatic ROOT TTree creation

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
  - [General Particle Source (GPS)](#general-particle-source-gps)
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
│   ├── geometry.json          # Default geometry
│   ├── geometry_v2.json       # Version 2
│   ├── geometry_v3.json       # Version 3
│   ├── geometry_v4.json       # Version 4 (includes boolean/union solids)
│   ├── geometry-4.json
│   ├── geometry_all.json
│   └── materials.json
└── webapp/                    # Local web dashboard (FastAPI)
    ├── app.py                 # Backend server
    ├── requirements.txt       # Python dependencies
    ├── templates/             # HTML pages
    ├── static/                # CSS & JavaScript
    ├── services/              # Geometry mesh helpers (incl. CSG booleans)
    ├── routers/               # REST API endpoints
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
| **Config** | Select geometry file (or upload a new JSON), configure the General Particle Source, set run parameters |
| **Run** | Start / stop simulation, live progress bar, streaming log, and run history |
| **Results** | Browse completed runs, download ROOT / log files, plot histograms, or view a 3D hit map overlaid with geometry (boolean solids rendered as true CSG meshes) |

### Config Tab — General Particle Source

The Config tab exposes the full power of the Geant4 General Particle Source (GPS) through a clean, dynamic form. Only the fields relevant to your current selections are shown.

**Top-level controls** (always visible):

| Field | Options |
|---|---|
| Particle | gamma, e-, e+, proton, neutron, mu-, mu+, alpha, opticalphoton |
| Energy type | Mono, Linear, Power-law, Gaussian |
| Energy | Value + unit (eV, keV, MeV, GeV) |
| Source | Point, Volume, Surface, Beam |
| Angular | Isotropic, Cosine-law, Focused, Beam 1D, Beam 2D |

**Conditionally shown fields:**

| Selection | Additional fields shown |
|---|---|
| Energy type = **Gaussian** | Sigma |
| Energy type = **Linear** | Min/Max energy, Gradient, Intercept |
| Energy type = **Power-law** | Min/Max energy, Alpha (power index) |
| Source = **Point** | Centre X/Y/Z, Position unit |
| Source = **Volume** | + Shape (Cylinder/Sphere/Box/…), dimensions, **Confine to volume** dropdown |
| Source = **Surface** | + Shape, dimensions |
| Source = **Beam** | Centre X/Y/Z, Position unit |
| Angular = **Cosine-law** | Min/Max θ, Min/Max φ |
| Angular = **Focused** | Focus point X/Y/Z |
| Angular = **Beam 1D/2D** | Direction X/Y/Z |

The **Confine to volume** dropdown (visible when Source = Volume) is automatically populated with the physical volume names from the selected geometry file. This lets you confine random source positions to a specific detector volume.

Each run is saved in `webapp/runs/<timestamp>/` with the generated macro, metadata, ROOT output, and log.

> **Note:** The dashboard is for local use only. It spawns `G4sim` as a subprocess on your machine.

The command-line workflow (`build/G4sim macros/batch.mac`) continues to work exactly as before.

---

## Configuration

### Geometry Files

The detector geometry is defined in JSON files under `config/`. These files describe volumes, materials, positions, and rotations — and can be created or edited with the companion [Geant4 Geometry Editor](https://github.com/acolijn/geant4-geometry-editor).

Supported volume types:

- **Primitive shapes** — box, cylinder, sphere, ellipsoid, torus, trapezoid, polycone
- **Assemblies** — groups of volumes placed together via `G4AssemblyVolume`, with multiple placements and nested hierarchies
- **Boolean solids** — union and subtraction of primitives via `G4UnionSolid` / `G4SubtractionSolid`; components listed in a `components` array with `boolean_operation` per component

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

### General Particle Source (GPS)

The simulation uses the Geant4 **General Particle Source** (GPS). GPS is far more flexible than the simple particle gun and supports point, volume, and surface sources, arbitrary energy spectra, and configurable angular distributions — all via `/gps/` macro commands at run-time.

#### Quick examples

**Mono-energetic point source** (equivalent to the old particle gun):

```
/gps/particle gamma
/gps/ene/type Mono
/gps/ene/mono 1 MeV
/gps/pos/type Point
/gps/pos/centre -10 0 0 cm
/gps/ang/type iso
```

**Volume source confined to a detector volume:**

```
/gps/particle gamma
/gps/ene/type Mono
/gps/ene/mono 662 keV
/gps/pos/type Volume
/gps/pos/shape Cylinder
/gps/pos/centre 0 0 0 cm
/gps/pos/radius 5 cm
/gps/pos/halfz 10 cm
/gps/pos/confine Box_1
/gps/ang/type iso
```

**Gaussian energy spectrum:**

```
/gps/particle e-
/gps/ene/type Gauss
/gps/ene/mono 1 MeV
/gps/ene/sigma 50 keV
/gps/pos/type Point
/gps/pos/centre 0 0 0 cm
/gps/ang/type iso
```

#### GPS command reference

| Category | Command | Description |
|---|---|---|
| Particle | `/gps/particle <name>` | Particle type (gamma, e-, proton, …) |
| Energy | `/gps/ene/type Mono` | Mono-energetic |
| | `/gps/ene/mono <E> <unit>` | Energy value |
| | `/gps/ene/type Gauss` | Gaussian distribution |
| | `/gps/ene/sigma <σ> <unit>` | Standard deviation |
| | `/gps/ene/type Lin` | Linear spectrum |
| | `/gps/ene/type Pow` | Power-law spectrum |
| | `/gps/ene/min` / `max` | Energy range (Lin, Pow) |
| Position | `/gps/pos/type Point` | Point source |
| | `/gps/pos/type Volume` | Volume source |
| | `/gps/pos/type Surface` | Surface source |
| | `/gps/pos/shape <shape>` | Cylinder, Sphere, Box, … |
| | `/gps/pos/centre <x y z> <unit>` | Source centre |
| | `/gps/pos/radius`, `halfz`, … | Shape dimensions |
| | `/gps/pos/confine <physVol>` | Restrict to a physical volume |
| Angular | `/gps/ang/type iso` | Isotropic |
| | `/gps/ang/type cos` | Cosine-law |
| | `/gps/ang/type focused` | Focused on a point |
| | `/gps/ang/mintheta`, `maxtheta` | Restrict polar angle |

> **Tip:** The web dashboard generates these macro commands for you — just pick the options in the form.

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
