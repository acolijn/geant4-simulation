import React, { useState } from 'react';
import { 
  Box, 
  Paper, 
  Typography, 
  Button,
  Tabs,
  Tab,
  TextField
} from '@mui/material';

const JsonViewer = ({ geometries, materials }) => {
  const [tabValue, setTabValue] = useState(0);
  
  // Format the JSON with proper indentation for display
  const formatJson = (data) => {
    return JSON.stringify(data, null, 2);
  };
  
  // Generate the geometry JSON
  const geometryJson = formatJson(geometries);
  
  // Generate the materials JSON
  const materialsJson = formatJson({ materials });
  
  // Handle download of JSON file
  const handleDownload = (content, filename) => {
    const blob = new Blob([content], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  };
  
  // Handle copy to clipboard
  const handleCopy = (content) => {
    navigator.clipboard.writeText(content);
  };

  return (
    <Paper sx={{ height: '100%', display: 'flex', flexDirection: 'column' }}>
      <Tabs 
        value={tabValue} 
        onChange={(e, newValue) => setTabValue(newValue)}
        variant="fullWidth"
      >
        <Tab label="Geometry JSON" />
        <Tab label="Materials JSON" />
      </Tabs>
      
      <Box sx={{ p: 2, display: 'flex', gap: 1 }}>
        <Button 
          variant="contained" 
          onClick={() => handleDownload(
            tabValue === 0 ? geometryJson : materialsJson,
            tabValue === 0 ? 'geometry.json' : 'materials.json'
          )}
        >
          Download JSON
        </Button>
        <Button 
          variant="outlined" 
          onClick={() => handleCopy(tabValue === 0 ? geometryJson : materialsJson)}
        >
          Copy to Clipboard
        </Button>
      </Box>
      
      <Box sx={{ flexGrow: 1, overflow: 'auto', p: 2 }}>
        <TextField
          multiline
          fullWidth
          variant="outlined"
          value={tabValue === 0 ? geometryJson : materialsJson}
          InputProps={{
            readOnly: true,
            style: { 
              fontFamily: 'monospace', 
              fontSize: '0.875rem',
              whiteSpace: 'pre',
              overflowWrap: 'normal',
              overflowX: 'auto'
            }
          }}
          sx={{ height: '100%' }}
        />
      </Box>
    </Paper>
  );
};

export default JsonViewer;
