import { useState, useEffect } from 'react';
import { 
  Box, 
  CssBaseline, 
  AppBar, 
  Toolbar, 
  Typography, 
  Tabs, 
  Tab,
  Container,
  ThemeProvider,
  createTheme
} from '@mui/material';
import Viewer3D from './components/Viewer3D';
import GeometryEditor from './components/GeometryEditor';
import MaterialsEditor from './components/MaterialsEditor';
import JsonViewer from './components/JsonViewer';
import './App.css';

// Create a theme
const theme = createTheme({
  palette: {
    primary: {
      main: '#1976d2',
    },
    secondary: {
      main: '#dc004e',
    },
  },
});

// Default geometry structure
const defaultGeometry = {
  world: {
    type: 'box',
    name: 'World',
    material: 'G4_AIR',
    size: {
      x: 200.0,
      y: 200.0,
      z: 200.0,
      unit: 'cm'
    },
    position: {
      x: 0.0,
      y: 0.0,
      z: 0.0,
      unit: 'cm'
    },
    rotation: {
      x: 0.0,
      y: 0.0,
      z: 0.0,
      unit: 'deg'
    }
  },
  volumes: []
};

// Default materials from the sample file
const defaultMaterials = {
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
  },
  "G4_AIR": {
    "type": "nist",
    "name": "G4_AIR"
  },
  "G4_WATER": {
    "type": "nist",
    "name": "G4_WATER"
  },
  "G4_Si": {
    "type": "nist",
    "name": "G4_Si"
  },
  "G4_Cf": {
    "type": "nist",
    "name": "G4_Cf"
  },
  "G4_Al": {
    "type": "nist",
    "name": "G4_Al"
  },
  "G4_Cu": {
    "type": "nist",
    "name": "G4_Cu"
  }
};

function App() {
  const [tabValue, setTabValue] = useState(0);
  const [geometries, setGeometries] = useState(defaultGeometry);
  const [materials, setMaterials] = useState(defaultMaterials);
  const [selectedGeometry, setSelectedGeometry] = useState(null);
  
  // Try to load saved geometries and materials from localStorage on initial load
  useEffect(() => {
    try {
      const savedGeometries = localStorage.getItem('geant4-geometries');
      if (savedGeometries) {
        setGeometries(JSON.parse(savedGeometries));
      }
      
      const savedMaterials = localStorage.getItem('geant4-materials');
      if (savedMaterials) {
        setMaterials(JSON.parse(savedMaterials));
      }
    } catch (error) {
      console.error('Error loading saved data:', error);
    }
  }, []);
  
  // Save geometries and materials to localStorage whenever they change
  useEffect(() => {
    try {
      localStorage.setItem('geant4-geometries', JSON.stringify(geometries));
      localStorage.setItem('geant4-materials', JSON.stringify(materials));
    } catch (error) {
      console.error('Error saving data:', error);
    }
  }, [geometries, materials]);
  
  // Handle updating a geometry
  const handleUpdateGeometry = (id, updatedObject, keepSelected = true) => {
    // First update the geometry
    if (id === 'world') {
      setGeometries({
        ...geometries,
        world: updatedObject
      });
    } else if (id.startsWith('volume-')) {
      const index = parseInt(id.split('-')[1]);
      const updatedVolumes = [...geometries.volumes];
      updatedVolumes[index] = updatedObject;
      
      setGeometries({
        ...geometries,
        volumes: updatedVolumes
      });
    }
    
    // Always ensure the selection stays on the object that was just updated
    // when keepSelected is true (which is the default for transform operations)
    if (keepSelected) {
      // Use setTimeout to ensure this runs after the state update
      setTimeout(() => {
        setSelectedGeometry(id);
      }, 0);
    }
  };
  
  // Handle adding a new geometry
  const handleAddGeometry = (newGeometry) => {
    setGeometries({
      ...geometries,
      volumes: [...geometries.volumes, newGeometry]
    });
    
    // Select the newly added geometry
    setSelectedGeometry(`volume-${geometries.volumes.length}`);
  };
  
  // Handle removing a geometry
  const handleRemoveGeometry = (id) => {
    if (id.startsWith('volume-')) {
      const index = parseInt(id.split('-')[1]);
      const updatedVolumes = [...geometries.volumes];
      updatedVolumes.splice(index, 1);
      
      setGeometries({
        ...geometries,
        volumes: updatedVolumes
      });
      
      setSelectedGeometry(null);
    }
  };
  
  // Handle updating materials
  const handleUpdateMaterials = (updatedMaterials) => {
    setMaterials(updatedMaterials);
  };

  return (
    <ThemeProvider theme={theme}>
      <CssBaseline />
      <Box sx={{ display: 'flex', flexDirection: 'column', height: '100vh' }}>
        <AppBar position="static">
          <Toolbar>
            <Typography variant="h6" component="div" sx={{ flexGrow: 1 }}>
              Geant4 Geometry Editor
            </Typography>
          </Toolbar>
          <Tabs 
            value={tabValue} 
            onChange={(e, newValue) => setTabValue(newValue)}
            centered
          >
            <Tab label="Geometry" />
            <Tab label="Materials" />
            <Tab label="JSON" />
          </Tabs>
        </AppBar>
        
        <Box sx={{ flexGrow: 1, overflow: 'hidden' }}>
          {/* Geometry Tab */}
          {tabValue === 0 && (
            <Box sx={{ display: 'flex', height: '100%' }}>
              <Box sx={{ width: '70%', height: '100%' }}>
                <Viewer3D 
                  geometries={geometries} 
                  selectedGeometry={selectedGeometry}
                  onSelect={setSelectedGeometry}
                  onUpdateGeometry={handleUpdateGeometry}
                />
              </Box>
              <Box sx={{ width: '30%', height: '100%', overflow: 'auto' }}>
                <GeometryEditor 
                  geometries={geometries}
                  materials={materials}
                  selectedGeometry={selectedGeometry}
                  onUpdateGeometry={handleUpdateGeometry}
                  onAddGeometry={handleAddGeometry}
                  onRemoveGeometry={handleRemoveGeometry}
                />
              </Box>
            </Box>
          )}
          
          {/* Materials Tab */}
          {tabValue === 1 && (
            <Container maxWidth="lg" sx={{ height: '100%', py: 2 }}>
              <MaterialsEditor 
                materials={materials}
                onUpdateMaterials={handleUpdateMaterials}
              />
            </Container>
          )}
          
          {/* JSON Tab */}
          {tabValue === 2 && (
            <Container maxWidth="lg" sx={{ height: '100%', py: 2 }}>
              <JsonViewer 
                geometries={geometries}
                materials={materials}
              />
            </Container>
          )}
        </Box>
      </Box>
    </ThemeProvider>
  );
}

export default App;





