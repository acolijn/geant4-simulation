# Geant4 Simulation — Neutron Transport

A Geant4 application for simulating neutron transport using high-precision neutron physics (FTFP_BERT_HP). The detector geometry is defined via JSON configuration files, making it easy to modify without recompiling.

[![Documentation Status](https://readthedocs.org/projects/geant4-simulation/badge/?version=latest)](https://geant4-simulation.readthedocs.io/en/latest/?badge=latest)

## Requirements

- Geant4 11.x (with HP neutron data files)
- ROOT 6.x
- CMake 3.16+
- nlohmann_json

All dependencies are available via conda (recommended).

## Quick Start with Conda

```bash
# 1. Create the environment
conda env create -f environment.yml

# 2. Activate it
conda activate g4

# 3. Build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Running the Simulation

From the project root directory:

```bash
# Interactive mode with visualization (Qt)
build/GeoTest

# Batch mode — no visualization, 1000 neutron events
build/GeoTest macros/novis.mac

# Batch mode — 100 neutron events
build/GeoTest macros/run_batch.mac

# Batch mode — with materials file
build/GeoTest macros/batch.mac
```

## Project Structure

```
├── GeoTest.cc              # Main application
├── CMakeLists.txt          # Build configuration
├── environment.yml         # Conda environment specification
├── include/                # Header files
├── src/                    # Source files
├── macros/                 # Geant4 macro files
│   ├── vis.mac             # Interactive visualization
│   ├── novis.mac           # Batch mode (no visualization)
│   ├── batch.mac           # Batch mode with materials config
│   ├── run_batch.mac       # Batch mode (100 events)
│   └── pre_init.mac        # Pre-initialization commands
├── config/                 # Detector configuration (JSON)
│   ├── geometry.json       # Default geometry definition
│   ├── geometry_all.json   # Extended geometry
│   ├── geometry_union.json # Union solid geometry
│   └── materials.json      # Material definitions
├── Dockerfile              # Docker image build
└── docker-compose.yml      # Docker Compose configuration
```

## Configuration

The detector geometry is defined in JSON files under `config/`. Select which geometry to use in your macro:

```
/detector/setGeometryFile config/geometry.json
/detector/setMaterialsFile config/materials.json
```

## Output

The simulation produces a ROOT file `GeoTest.root` containing a TTree `GeoTestTree` with the following branches:

| Branch | Type | Description |
|---|---|---|
| `EnergyDeposit` | `Double_t` | Total energy deposit (MeV) |
| `TrackLength` | `Double_t` | Track length (mm) |
| `NeutronCount` | `Int_t` | Number of neutrons |

Hit-level information (position, volume, time, energy per step) is printed to stdout during the run.

## Docker

You can also build and run via Docker:

```bash
# Build and run with default macro
docker compose up --build

# Run with a specific macro
docker compose run --rm geant4-sim macros/batch.mac
```

## Documentation

Full documentation is available on [Read the Docs](https://geant4-simulation.readthedocs.io/en/latest/).
