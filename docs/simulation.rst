Simulation Details
=================

Geant4 simulation to demonstrate geometry editor integration.

Geometry Editor Integration
-------------------------

Get4 now integrates with a web-based geometry editor that allows you to:

- Create and edit detector geometries visually
- Export geometry configurations as JSON files
- Preview the 3D representation of your detector

The geometry editor ensures consistent coordinate systems with Geant4:

- Z-axis points upward
- Cylinders are created with their circular face in the X-Y plane
- Height extends along the Z-axis
- Rotations follow Geant4's sequential rotation system

Unit Handling
------------

The simulation now properly handles units for all geometry dimensions:

- Length units: "mm", "cm", "m" (default is "mm" if not specified)
- Angle units: "deg", "rad" (default is "deg" if not specified)

Units can be specified in two ways:

1. As a property of vectors (position, size)
2. As a direct property of volumes (radius, height)

Rotation System
--------------

Rotations are applied in the Geant4 sequence:

1. First rotation around X axis
2. Then rotation around the new Y axis
3. Finally rotation around the new Z axis

This sequential rotation system ensures consistent behavior between the geometry editor and the Geant4 simulation.

Primary Generator
----------------

The simulation uses a particle gun to generate primary neutrons:

- Default energy: 1 MeV
- Direction: Along positive Z axis
- Starting position: Random on top face of world volume

Data Collection
--------------

For each event, the following data is collected and stored in a ROOT file:

- Energy deposits in the liquid xenon
- Track lengths
- Neutron counts

The data is saved in a TTree format for easy analysis.

For detailed API documentation of the simulation components, see :doc:`api`.
