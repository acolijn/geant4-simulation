Simulation Details
=================

Geant4 simulation to demonstrate geometry editor integration.

Geometry Editor Integration
-------------------------

The simulation integrates with a web-based geometry editor that allows you to:

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

The simulation properly handles units for all geometry dimensions:

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

General Particle Source (GPS)
----------------------------

The simulation uses the Geant4 **General Particle Source** (GPS) instead of
the simple particle gun.  GPS is configured entirely at run-time via ``/gps/``
macro commands and supports:

* **Point, volume, and surface sources** — generate particles from a single
  point, randomly inside a volume, or on a surface.
* **Volume confinement** — restrict the random positions to a specific
  physical volume (``/gps/pos/confine <name>``).
* **Energy spectra** — mono-energetic, Gaussian, linear, power-law, or
  user-defined histograms.
* **Angular distributions** — isotropic, cosine-law, focused toward a point,
  or pencil-beam.
* **Multiple sources** — overlay several independent sources with individual
  intensities.

Quick example — mono-energetic gamma point source:

.. code-block:: text

   /gps/particle gamma
   /gps/ene/type Mono
   /gps/ene/mono 1 MeV
   /gps/pos/type Point
   /gps/pos/centre -10 0 0 cm
   /gps/ang/type iso

Quick example — volume source confined to a detector:

.. code-block:: text

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

.. tip::

   The **web dashboard** generates these macro commands automatically — just
   fill in the form on the *Config* tab.

Data Collection
--------------

For each event, the following data is collected and stored in a ROOT file:

- Energy deposits in sensitive detector volumes
- Hit positions (x, y, z)
- Volume names for each hit

The data is saved in a TTree format for easy analysis.

For detailed API documentation of the simulation components, see :doc:`api`.
