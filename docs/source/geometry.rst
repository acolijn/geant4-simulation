Geometry Configuration
=====================

The detector geometry is configured through JSON files, making it easy to modify
without recompiling the code.  JSON files can be created and edited with the
companion `Geant4 Geometry Editor <https://github.com/acolijn/geant4-geometry-editor>`_.

Supported Volume Types
---------------------

The geometry parser supports the following solid types:

* **Primitive shapes** — box, cylinder, sphere, ellipsoid, torus, trapezoid, polycone
* **Assemblies** — groups of volumes placed together via ``G4AssemblyVolume`` /
  ``MakeImprint``, with support for multiple placements and nested hierarchies
* **Boolean solids** — union, subtraction, and intersection of primitive shapes
  via ``G4UnionSolid`` / ``G4SubtractionSolid``; components are listed in a
  ``components`` array with a ``boolean_operation`` field per component

Configuration Files
------------------

A single JSON file contains everything needed for a simulation:

1. **world** — the top-level world volume (always a box)
2. **volumes** — all daughter volumes, assemblies, and boolean solids
3. **materials** — material definitions (NIST, element-based, or compounds)

JSON Format
-----------

The geometry configuration uses a hierarchical JSON format with ``placements``
arrays to support multiple placements of the same volume definition:

.. code-block:: json

    {
        "world": {
            "name": "World",
            "type": "box",
            "material": "G4_AIR",
            "dimensions": { "x": 2000, "y": 2000, "z": 2000 },
            "placements": [
                { "x": 0, "y": 0, "z": 0, "rotation": { "x": 0, "y": 0, "z": 0 } }
            ]
        },
        "volumes": [
            {
                "name": "Detector",
                "g4name": "Detector",
                "type": "cylinder",
                "material": "G4_Si",
                "dimensions": { "radius": 50, "height": 100 },
                "placements": [
                    {
                        "name": "Detector",
                        "g4name": "Detector",
                        "x": 0, "y": 0, "z": 0,
                        "rotation": { "x": 0, "y": 0, "z": 0 },
                        "parent": "World"
                    }
                ],
                "visible": true,
                "hitsCollectionName": "MyHitsCollection"
            }
        ],
        "materials": {
            "G4_AIR": { "type": "nist", "density": 0.00120479, "density_unit": "g/cm3" },
            "G4_Si":  { "type": "nist", "density": 2.33, "density_unit": "g/cm3" }
        }
    }

.. note::

   All dimensions are in **mm** and all angles in **radians** in the current
   JSON format produced by the Geometry Editor.  The parser also accepts
   explicit ``unit`` fields (``"mm"``, ``"cm"``, ``"m"``, ``"deg"``, ``"rad"``).

Boolean Solid Example
^^^^^^^^^^^^^^^^^^^^^

A boolean (union) solid with a box and a subtracted cylinder:

.. code-block:: json

    {
        "name": "MyUnion",
        "g4name": "MyUnion",
        "type": "union",
        "material": "G4_AIR",
        "placements": [
            { "name": "MyUnion", "g4name": "MyUnion",
              "x": 0, "y": 0, "z": 0,
              "rotation": { "x": 0, "y": 0, "z": 0 },
              "parent": "World" }
        ],
        "components": [
            {
                "name": "BaseBox", "type": "box",
                "dimensions": { "x": 100, "y": 100, "z": 100 },
                "placements": [{ "x": 0, "y": 0, "z": 0, "rotation": { "x": 0, "y": 0, "z": 0 } }],
                "boolean_operation": "union"
            },
            {
                "name": "SubCyl", "type": "cylinder",
                "dimensions": { "radius": 30, "height": 120 },
                "placements": [{ "x": 0, "y": 0, "z": 0, "rotation": { "x": 0, "y": 0, "z": 0 } }],
                "boolean_operation": "subtract"
            }
        ]
    }

Units
-----

All dimensions in the geometry configuration can specify units:

- Length units: "mm", "cm", "m" (default is "mm" if not specified)
- Angle units: "deg", "rad" (default is "deg" if not specified)

For shape-specific dimensions (like radius, height), you can specify the unit in two ways:

1. As a property of the vector (for position, size):

   .. code-block:: json

       "position": {
           "x": 0.0,
           "y": 0.0,
           "z": 0.0,
           "unit": "cm"
       }

2. As a direct property of the volume (for radius, height, etc.):

   .. code-block:: json

       "radius": 5.0,
       "height": 10.0,
       "unit": "cm"

Rotations
---------

Rotations are specified in the following format:

.. code-block:: json

    "rotation": {
        "x": 30.0,
        "y": 0.0,
        "z": 45.0,
        "unit": "deg"
    }

Rotations are applied in the Geant4 sequence:

1. First rotation around X axis
2. Then rotation around the new Y axis
3. Finally rotation around the new Z axis

Cylinder Orientation
-------------------

Cylinders in Geant4 are created with:

- The circular face in the X-Y plane
- The height extending along the Z-axis

This means that rotations will behave as follows:

- X rotation: Tilts the cylinder around the X axis
- Y rotation: Tilts the cylinder around the Y axis
- Z rotation: Rotates the cylinder around its central axis (Z)

For detailed API documentation of the geometry parser, see :doc:`api`.
