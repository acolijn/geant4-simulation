import React, { useState } from 'react';
import { 
  Box, 
  Paper, 
  Typography, 
  TextField, 
  Select, 
  MenuItem, 
  FormControl, 
  InputLabel,
  Button,
  Tabs,
  Tab,
  Divider
} from '@mui/material';

const GeometryEditor = ({ 
  geometries, 
  materials, 
  selectedGeometry, 
  onUpdateGeometry, 
  onAddGeometry, 
  onRemoveGeometry 
}) => {
  const [tabValue, setTabValue] = useState(0);
  const [newGeometryType, setNewGeometryType] = useState('box');

  // Get the selected geometry object
  const getSelectedGeometryObject = () => {
    if (!selectedGeometry) return null;
    if (selectedGeometry === 'world') return geometries.world;
    if (selectedGeometry.startsWith('volume-')) {
      const index = parseInt(selectedGeometry.split('-')[1]);
      return geometries.volumes[index];
    }
    return null;
  };

  const selectedObject = getSelectedGeometryObject();

  // Handle property changes
  const handlePropertyChange = (property, value, allowNegative = true) => {
    if (!selectedGeometry) return;
    
    const updatedObject = { ...getSelectedGeometryObject() };
    
    // Process numeric values
    let processedValue = value;
    if (typeof value === 'string' && value.match(/^-?\d*\.?\d*$/)) {
      // Convert to number if it's a valid numeric string
      const numValue = parseFloat(value);
      
      // If not allowing negative values, ensure it's positive
      if (!allowNegative && numValue < 0) {
        processedValue = 0;
      } else if (!isNaN(numValue)) {
        processedValue = numValue;
      } else if (value === '-' || value === '') {
        // Allow typing a minus sign or empty string (will be converted to 0 later)
        processedValue = value;
      } else {
        processedValue = 0;
      }
    }
    
    // Handle nested properties like position.x
    if (property.includes('.')) {
      const [parent, child] = property.split('.');
      
      // For empty string or just a minus sign, keep it as is to allow typing
      if (processedValue === '' || processedValue === '-') {
        updatedObject[parent] = { 
          ...updatedObject[parent], 
          [child]: processedValue 
        };
      } else {
        updatedObject[parent] = { 
          ...updatedObject[parent], 
          [child]: parseFloat(processedValue) || 0 
        };
      }
    } else {
      // For empty string or just a minus sign, keep it as is to allow typing
      if (processedValue === '' || processedValue === '-') {
        updatedObject[property] = processedValue;
      } else if (typeof processedValue === 'number') {
        updatedObject[property] = processedValue;
      } else {
        updatedObject[property] = parseFloat(processedValue) || 0;
      }
    }
    
    onUpdateGeometry(selectedGeometry, updatedObject);
  };

  // Add a new geometry
  const handleAddGeometry = () => {
    const newGeometry = {
      type: newGeometryType,
      name: `New${newGeometryType.charAt(0).toUpperCase() + newGeometryType.slice(1)}`,
      material: 'G4_AIR',
      position: { x: 0, y: 0, z: 0, unit: 'cm' },
      rotation: { x: 0, y: 0, z: 0, unit: 'deg' },
      mother_volume: 'World'
    };
    
    // Add specific properties based on geometry type
    if (newGeometryType === 'box') {
      newGeometry.size = { x: 10, y: 10, z: 10, unit: 'cm' };
    } else if (newGeometryType === 'cylinder') {
      newGeometry.radius = 5;
      newGeometry.height = 10;
      newGeometry.inner_radius = 0;
    } else if (newGeometryType === 'sphere') {
      newGeometry.radius = 5;
    }
    
    onAddGeometry(newGeometry);
  };

  // Render the property editor for the selected geometry
  const renderPropertyEditor = () => {
    if (!selectedObject) {
      return (
        <Typography variant="body1" sx={{ p: 2 }}>
          Select a geometry to edit its properties
        </Typography>
      );
    }

    return (
      <Box sx={{ p: 2 }}>
        <Typography variant="h6">
          {selectedObject.name || 'Unnamed Geometry'}
        </Typography>
        
        <TextField
          label="Name"
          value={selectedObject.name || ''}
          onChange={(e) => handlePropertyChange('name', e.target.value)}
          fullWidth
          margin="normal"
          size="small"
        />
        
        <FormControl fullWidth margin="normal" size="small">
          <InputLabel>Material</InputLabel>
          <Select
            value={selectedObject.material || ''}
            label="Material"
            onChange={(e) => handlePropertyChange('material', e.target.value)}
          >
            {Object.keys(materials).map((material) => (
              <MenuItem key={material} value={material}>
                {material}
              </MenuItem>
            ))}
          </Select>
        </FormControl>
        
        <Typography variant="subtitle1" sx={{ mt: 2 }}>Position</Typography>
        <Box sx={{ display: 'flex', gap: 1 }}>
          <TextField
            label="X"
            type="number"
            value={selectedObject.position?.x || 0}
            onChange={(e) => handlePropertyChange('position.x', e.target.value)}
            size="small"
            inputProps={{ step: 'any' }}
          />
          <TextField
            label="Y"
            type="number"
            value={selectedObject.position?.y || 0}
            onChange={(e) => handlePropertyChange('position.y', e.target.value)}
            size="small"
            inputProps={{ step: 'any' }}
          />
          <TextField
            label="Z"
            type="number"
            value={selectedObject.position?.z || 0}
            onChange={(e) => handlePropertyChange('position.z', e.target.value)}
            size="small"
            inputProps={{ step: 'any' }}
          />
          <TextField
            label="Unit"
            value={selectedObject.position?.unit || 'cm'}
            onChange={(e) => handlePropertyChange('position.unit', e.target.value)}
            size="small"
          />
        </Box>
        
        <Typography variant="subtitle1" sx={{ mt: 2 }}>Rotation</Typography>
        <Box sx={{ display: 'flex', gap: 1 }}>
          <TextField
            label="X"
            type="number"
            value={selectedObject.rotation?.x || 0}
            onChange={(e) => handlePropertyChange('rotation.x', e.target.value)}
            size="small"
            inputProps={{ step: 'any' }}
          />
          <TextField
            label="Y"
            type="number"
            value={selectedObject.rotation?.y || 0}
            onChange={(e) => handlePropertyChange('rotation.y', e.target.value)}
            size="small"
            inputProps={{ step: 'any' }}
          />
          <TextField
            label="Z"
            type="number"
            value={selectedObject.rotation?.z || 0}
            onChange={(e) => handlePropertyChange('rotation.z', e.target.value)}
            size="small"
            inputProps={{ step: 'any' }}
          />
          <TextField
            label="Unit"
            value={selectedObject.rotation?.unit || 'deg'}
            onChange={(e) => handlePropertyChange('rotation.unit', e.target.value)}
            size="small"
          />
        </Box>
        
        {/* Render type-specific properties */}
        {selectedObject.type === 'box' && (
          <>
            <Typography variant="subtitle1" sx={{ mt: 2 }}>Size</Typography>
            <Box sx={{ display: 'flex', gap: 1 }}>
              <TextField
                label="X"
                type="number"
                value={selectedObject.size?.x || 0}
                onChange={(e) => handlePropertyChange('size.x', e.target.value, false)}
                size="small"
                inputProps={{ step: 'any', min: 0 }}
              />
              <TextField
                label="Y"
                type="number"
                value={selectedObject.size?.y || 0}
                onChange={(e) => handlePropertyChange('size.y', e.target.value, false)}
                size="small"
                inputProps={{ step: 'any', min: 0 }}
              />
              <TextField
                label="Z"
                type="number"
                value={selectedObject.size?.z || 0}
                onChange={(e) => handlePropertyChange('size.z', e.target.value, false)}
                size="small"
                inputProps={{ step: 'any', min: 0 }}
              />
              <TextField
                label="Unit"
                value={selectedObject.size?.unit || 'cm'}
                onChange={(e) => handlePropertyChange('size.unit', e.target.value)}
                size="small"
              />
            </Box>
          </>
        )}
        
        {selectedObject.type === 'cylinder' && (
          <>
            <Typography variant="subtitle1" sx={{ mt: 2 }}>Dimensions</Typography>
            <Box sx={{ display: 'flex', gap: 1, mb: 1 }}>
              <TextField
                label="Radius"
                type="number"
                value={selectedObject.radius || 0}
                onChange={(e) => handlePropertyChange('radius', e.target.value, false)}
                size="small"
                inputProps={{ step: 'any', min: 0 }}
              />
              <TextField
                label="Height"
                type="number"
                value={selectedObject.height || 0}
                onChange={(e) => handlePropertyChange('height', e.target.value, false)}
                size="small"
                inputProps={{ step: 'any', min: 0 }}
              />
            </Box>
            <TextField
              label="Inner Radius"
              type="number"
              value={selectedObject.innerRadius || 0}
              onChange={(e) => handlePropertyChange('innerRadius', e.target.value, false)}
              size="small"
              sx={{ mb: 1 }}
              inputProps={{ step: 'any', min: 0 }}
            />
          </>
        )}
        
        {selectedObject.type === 'sphere' && (
          <>
            <Typography variant="subtitle1" sx={{ mt: 2 }}>Dimensions</Typography>
            <TextField
              label="Radius"
              type="number"
              value={selectedObject.radius || 0}
              onChange={(e) => handlePropertyChange('radius', e.target.value, false)}
              size="small"
              fullWidth
              inputProps={{ step: 'any', min: 0 }}
            />
          </>
        )}
        
        {selectedGeometry !== 'world' && (
          <Box sx={{ mt: 2 }}>
            <Button 
              variant="outlined" 
              color="error" 
              onClick={() => onRemoveGeometry(selectedGeometry)}
            >
              Remove Geometry
            </Button>
          </Box>
        )}
      </Box>
    );
  };

  // Render the "Add New" tab content
  const renderAddNewTab = () => {
    return (
      <Box sx={{ p: 2 }}>
        <Typography variant="h6">Add New Geometry</Typography>
        
        <FormControl fullWidth margin="normal">
          <InputLabel>Geometry Type</InputLabel>
          <Select
            value={newGeometryType}
            label="Geometry Type"
            onChange={(e) => setNewGeometryType(e.target.value)}
          >
            <MenuItem value="box">Box</MenuItem>
            <MenuItem value="cylinder">Cylinder</MenuItem>
            <MenuItem value="sphere">Sphere</MenuItem>
          </Select>
        </FormControl>
        
        <Button 
          variant="contained" 
          color="primary" 
          onClick={handleAddGeometry}
          sx={{ mt: 2 }}
          fullWidth
        >
          Add Geometry
        </Button>
      </Box>
    );
  };

  return (
    <Paper sx={{ height: '100%', display: 'flex', flexDirection: 'column' }}>
      <Tabs 
        value={tabValue} 
        onChange={(e, newValue) => setTabValue(newValue)}
        variant="fullWidth"
      >
        <Tab label="Properties" />
        <Tab label="Add New" />
      </Tabs>
      <Divider />
      <Box sx={{ flexGrow: 1, overflow: 'auto' }}>
        {tabValue === 0 ? renderPropertyEditor() : renderAddNewTab()}
      </Box>
    </Paper>
  );
};

export default GeometryEditor;
