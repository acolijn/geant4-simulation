// Add this to the top of your Viewer3D.jsx file to enable more detailed logging
console.log = function() {
  const args = Array.from(arguments);
  const timestamp = new Date().toISOString();
  console.error(`[${timestamp}] LOG:`, ...args);
};
