Geometry Configuration
=====================

The detector geometry in Windsurf is configured through JSON files, making it easy to modify without recompiling the code.

Configuration Files
------------------

Two main configuration files are used:

1. Geometry Configuration
   - Defines the physical structure of the detector
   - Specifies dimensions, positions, and rotations
   - Defines the hierarchy of volumes

2. Materials Configuration
   - Defines all materials used in the simulation
   - Specifies material properties and compositions

JSON Format
-----------

The geometry configuration uses a hierarchical JSON format:

.. code-block:: json

    {
        "world": {
            "name": "World",
            "material": "G4_AIR",
            "type": "box",
            "size": {
                "x": 2.0,
                "y": 2.0,
                "z": 2.0,
                "unit": "m"
            },
            "position": {
                "x": 0.0,
                "y": 0.0,
                "z": 0.0,
                "unit": "cm"
            },
            "rotation": {
                "x": 0.0,
                "y": 0.0,
                "z": 0.0,
                "unit": "deg"
            }
        },
        "volumes": [
            {
                "name": "Detector",
                "type": "cylinder",
                "material": "G4_Si",
                "radius": 5.0,
                "height": 10.0,
                "unit": "cm",
                "position": {
                    "x": 0.0,
                    "y": 0.0,
                    "z": 0.0,
                    "unit": "cm"
                },
                "rotation": {
                    "x": 0.0,
                    "y": 0.0,
                    "z": 0.0,
                    "unit": "deg"
                },
                "mother_volume": "World"
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
