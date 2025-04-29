# Geant4 Geometry Editor

A web-based application for creating and editing Geant4 geometry JSON files with a graphical interface. This tool allows you to visually design detector geometries and export them as JSON files that can be used with Geant4.

## Features

- 3D visualization of Geant4 geometries using Three.js
- Support for basic shapes: Box, Cylinder, and Sphere
- Material editor for defining custom materials
- Real-time JSON preview and export
- Automatic saving of work in progress to localStorage

## Usage

### Geometry Tab

The Geometry tab is divided into two sections:

1. **3D Viewer** (left side): Visualizes your geometry in 3D. You can:
   - Rotate, pan, and zoom using mouse controls
   - Click on objects to select them for editing
   - View the world volume and all defined geometries

2. **Properties Panel** (right side): Edit properties of the selected geometry or add new shapes.
   - When a geometry is selected, you can modify its properties (position, rotation, size, material)
   - Use the "Add New" tab to create new geometry objects

### Materials Tab

The Materials tab allows you to manage materials for your geometries:

- View and edit existing materials
- Create new materials (NIST materials, element-based materials, or compounds)
- Define physical properties like density, state, and temperature
- Specify elemental composition for custom materials

### JSON Tab

The JSON tab provides a view of the generated JSON for both geometries and materials:

- View the JSON representation of your current design
- Copy JSON to clipboard
- Download JSON files for use with Geant4

## JSON Format

The application generates JSON files in the format expected by Geant4 JSON parsers:

### Geometry JSON

```json
{
  "world": {
    "type": "box",
    "name": "World",
    "material": "G4_AIR",
    "size": { "x": 2.0, "y": 2.0, "z": 2.0, "unit": "m" },
    "position": { "x": 0.0, "y": 0.0, "z": 0.0, "unit": "m" },
    "rotation": { "x": 0.0, "y": 0.0, "z": 0.0, "unit": "deg" }
  },
  "volumes": [
    {
      "type": "box",
      "name": "DetectorVolume",
      "material": "G4_Si",
      "size": { "x": 10.0, "y": 10.0, "z": 1.0, "unit": "cm" },
      "position": { "x": 0.0, "y": 0.0, "z": 0.0, "unit": "cm" },
      "rotation": { "x": 0.0, "y": 0.0, "z": 0.0, "unit": "deg" },
      "mother_volume": "World"
    }
  ]
}
```

### Materials JSON

```json
{
  "materials": {
    "G4_AIR": {
      "type": "nist",
      "name": "G4_AIR"
    },
    "LXe": {
      "type": "element_based",
      "density": 3.02,
      "density_unit": "g/cm3",
      "state": "liquid",
      "temperature": 165.0,
      "temperature_unit": "kelvin",
      "composition": {
        "Xe": 1
      }
    }
  }
}
```

## Development

This application is built with React and Vite, using Three.js for 3D visualization and Material UI for the user interface.

### Running the Application

```bash
# Navigate to the project directory
cd geant4-geometry-editor

# Install dependencies
npm install

# Start the development server
npm run dev
```

### Building for Production

```bash
npm run build
```
