Simulation Details
=================

Windsurf simulates neutron transport through a liquid xenon detector using Geant4.

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
