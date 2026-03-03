Web Dashboard
=============

The G4sim web dashboard is a local FastAPI application that provides a
browser-based interface for configuring, running, and analysing Geant4
simulations.

Starting the Dashboard
----------------------

.. code-block:: bash

   conda activate g4
   cd webapp
   uvicorn app:app --host 127.0.0.1 --port 8000

Then open **http://127.0.0.1:8000** in your browser.

.. note::

   The dashboard is intended for local use only.  It spawns ``G4sim`` as a
   subprocess on your machine.

Architecture
------------

::

   webapp/
   ├── app.py              # FastAPI application and page routes
   ├── config.py           # Shared paths and constants
   ├── requirements.txt    # Python dependencies
   ├── routers/
   │   ├── config_api.py   # Geometry listing, upload, volume introspection
   │   ├── run_api.py      # Start / stop / status, macro generation
   │   └── results_api.py  # File listing, ROOT branch listing, plotting
   ├── services/
   │   ├── simulation.py   # Process management (start, monitor, kill)
   │   └── geometry.py     # Geometry mesh helpers for Plotly visualisation
   │                       # (incl. true CSG booleans via trimesh + manifold3d)
   ├── templates/
   │   └── index.html      # Single-page HTML with Config / Run / Results tabs
   ├── static/
   │   ├── app.js          # Client-side logic (tab switching, form, API calls)
   │   └── style.css       # Styling
   └── runs/               # Auto-created per-run output directories (gitignored)


Tabs
----

Config
~~~~~~

The **Config** tab allows you to:

* **Select a geometry file** from ``config/`` or upload a new JSON file.
* **Configure the General Particle Source (GPS)** — the form dynamically
  shows only the fields relevant to your current selection.

The top-level form row contains five controls:

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Control
     - Description
   * - Particle
     - Particle type: gamma, e−, e+, proton, neutron, μ−, μ+, alpha,
       opticalphoton
   * - Energy type
     - ``Mono`` (single energy), ``Linear``, ``Power-law``, or ``Gaussian``
   * - Energy
     - Energy value and unit (eV, keV, MeV, GeV)
   * - Source
     - ``Point``, ``Volume``, ``Surface``, or ``Beam``
   * - Angular
     - ``Isotropic``, ``Cosine-law``, ``Focused``, ``Beam 1D``, ``Beam 2D``

Conditional fields appear when needed:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Selection
     - Additional fields
   * - Energy = Gaussian
     - Sigma
   * - Energy = Linear
     - Min / Max energy, Gradient, Intercept
   * - Energy = Power-law
     - Min / Max energy, Alpha (power index)
   * - Source = Point
     - Centre X / Y / Z, Position unit
   * - Source = Volume
     - Shape (Cylinder, Sphere, Box, …), dimensions, **Confine to volume**
       dropdown
   * - Source = Surface
     - Shape, dimensions
   * - Source = Beam
     - Centre X / Y / Z, Position unit
   * - Angular = Cosine-law
     - Min / Max θ, Min / Max φ
   * - Angular = Focused
     - Focus point X / Y / Z
   * - Angular = Beam 1D/2D
     - Direction X / Y / Z

Confine to Volume
^^^^^^^^^^^^^^^^^

When the source type is **Volume**, a *Confine to physical volume* dropdown
is shown.  This dropdown is automatically populated with the ``g4name`` values
extracted from the currently selected geometry JSON file (via the
``GET /api/geometries/{filename}/volumes`` endpoint).

Selecting a volume here adds the ``/gps/pos/confine <name>`` command to the
generated macro, restricting random source positions to lie within that
physical volume.

Run
~~~

The **Run** tab provides:

* **Start / Stop buttons** — launches ``G4sim`` with the auto-generated macro.
* **Live progress bar** — parsed from Geant4's ``>>> Event:`` log output.
* **Streaming log** — real-time output via a WebSocket connection.
* **Run history table** — lists all completed and running simulations.

Results
~~~~~~~

The **Results** tab lets you:

* Select a past run from the dropdown.
* **Download** the ROOT output, macro, log, or metadata files.
* **Plot histograms** — select a branch from the ROOT file and visualise it
  with Plotly.
* **3D hit map** — render all hit positions as a 3D scatter plot, overlaid
  with the detector geometry (including true CSG boolean solids rendered via
  ``trimesh`` + ``manifold3d``).


REST API
--------

.. list-table::
   :header-rows: 1
   :widths: 10 35 55

   * - Method
     - Endpoint
     - Description
   * - GET
     - ``/api/geometries``
     - List available geometry JSON files in ``config/``
   * - GET
     - ``/api/geometries/{filename}/volumes``
     - Return physical volume names (``g4name``) from a geometry file
   * - POST
     - ``/api/upload-geometry``
     - Upload a new geometry JSON to ``config/``
   * - POST
     - ``/api/run``
     - Start a simulation run (JSON body with GPS parameters)
   * - POST
     - ``/api/stop``
     - Kill the currently running simulation
   * - GET
     - ``/api/status``
     - Check whether a simulation is running and get recent log lines
   * - GET
     - ``/api/runs``
     - List completed runs with metadata
   * - GET
     - ``/api/results/{runId}/files``
     - List output files for a specific run
   * - GET
     - ``/api/results/{runId}/branches``
     - List ROOT TTree branches for a run's output file
   * - GET
     - ``/api/results/{runId}/plot/{branch}``
     - Return a Plotly histogram JSON for the given branch
   * - GET
     - ``/api/results/{runId}/plot3d``
     - Return a Plotly 3D scatter JSON of all hit positions
   * - GET
     - ``/api/results/{runId}/download/{filename}``
     - Download a specific output file
   * - GET
     - ``/api/results/{runId}/log``
     - Return the run's log text
   * - WS
     - ``/ws/log``
     - WebSocket for live log streaming during a run


Macro Generation
----------------

The dashboard generates Geant4 macros automatically from the form inputs.
A typical generated macro looks like:

.. code-block:: text

   /detector/setGeometryFile config/geometry-4.json
   /output/setFileName webapp/runs/20260301_143000/G4sim.root
   /run/initialize
   /vis/disable
   /hits/setVerbose 0

   # --- General Particle Source ---
   /gps/particle gamma
   /gps/ene/type Mono
   /gps/ene/mono 1 MeV

   /gps/pos/type Point
   /gps/pos/centre 0 0 0 cm

   /gps/ang/type iso

   /run/beamOn 10000

The macro is saved to the run directory as ``run.mac`` alongside the ROOT
output, metadata JSON, and log file.
