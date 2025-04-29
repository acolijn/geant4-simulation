# Neutron Transport in Liquid Xenon

This Geant4 application simulates neutron transport in a liquid xenon volume using high-precision neutron physics (HP).

## Requirements
- Geant4 (with HP data files)
- ROOT
- CMake

## Build Instructions

```bash
mkdir build
cd build
cmake ..
make
```

## Running the Application

Interactive mode:
```bash
./neutronLXe
```

Batch mode (with a macro file):
```bash
./neutronLXe run.mac
```

## Output
The simulation creates a ROOT file `neutron_data.root` containing:
- Energy deposits
- Track lengths
- Neutron counts

The data is stored in a TTree named "neutronTree".
