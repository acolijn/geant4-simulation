import React, { useState, useEffect, useRef, useCallback } from 'react';
import { Canvas, useThree, useFrame } from '@react-three/fiber';
import { OrbitControls, TransformControls } from '@react-three/drei';
import * as THREE from 'three';

// Coordinate system component
function CoordinateSystem() {
  const { scene } = useThree();
  
  useEffect(() => {
    const axesHelper = new THREE.AxesHelper(100);
    scene.add(axesHelper);
    
    return () => {
      scene.remove(axesHelper);
    };
  }, [scene]);
  
  return null;
}

// Camera setup component
function CameraSetup({ setFrontViewCamera }) {
  const { camera } = useThree();
  
  // Set front view on initial load
  useEffect(() => {
    camera.position.set(0, 0, 250);
    camera.lookAt(0, 0, 0);
    
    // Store the camera in the ref for the front view button
    if (setFrontViewCamera) {
      setFrontViewCamera(camera);
    }
  }, [camera, setFrontViewCamera]);
  
  return null;
}

// Custom TransformControls wrapper component
function CustomTransformControls({ object, mode, enabled, onTransformStart, onTransformEnd, onTransformChange }) {
  const transformRef = useRef();
  const { camera } = useThree();
  
  // Update mode when it changes
  useEffect(() => {
    if (transformRef.current) {
      transformRef.current.setMode(mode);
    }
  }, [mode]);
  
  // Setup event listeners
  useEffect(() => {
    const controls = transformRef.current;
    if (controls && enabled) {
      // Handle transform start
      const handleMouseDown = () => {
        if (onTransformStart) onTransformStart();
      };
      
      // Handle transform end
      const handleMouseUp = () => {
        if (onTransformEnd) onTransformEnd();
      };
      
      // Handle transform change (during dragging)
      const handleObjectChange = () => {
        if (onTransformChange) onTransformChange();
      };
      
      controls.addEventListener('mouseDown', handleMouseDown);
      controls.addEventListener('mouseUp', handleMouseUp);
      controls.addEventListener('objectChange', handleObjectChange);
      
      return () => {
        controls.removeEventListener('mouseDown', handleMouseDown);
        controls.removeEventListener('mouseUp', handleMouseUp);
        controls.removeEventListener('objectChange', handleObjectChange);
      };
    }
  }, [enabled, onTransformStart, onTransformEnd, onTransformChange]);
  
  return (
    <TransformControls
      ref={transformRef}
      object={object}
      mode={mode}
      size={0.75}
      camera={camera}
      enabled={enabled}
    />
  );
}

// TransformableObject component to handle transform controls
function TransformableObject({ object, objectKey, isSelected, transformMode, onSelect, onTransformEnd }) {
  const meshRef = useRef();
  const { gl } = useThree();
  
  // This tracks if we're currently transforming
  const [isTransforming, setIsTransforming] = useState(false);
  
  // Check if this is the World volume
  const isWorld = object.name === 'World';
  
  // Handle transform start
  const handleTransformStart = useCallback(() => {
    setIsTransforming(true);
    gl.domElement.style.cursor = 'grabbing';
    
    // Ensure this object stays selected
    onSelect(objectKey);
    
    // Disable orbit controls during transformation
    const controls = gl.domElement.parentElement?.__r3f?.controls;
    if (controls) {
      controls.enabled = false;
    }
  }, [gl, objectKey, onSelect]);
  
  // Handle transform change (during dragging)
  const handleTransformChange = useCallback(() => {
    if (!isTransforming || !meshRef.current) return;
    
    // Get current transform values
    const position = meshRef.current.position;
    const rotation = meshRef.current.rotation;
    const scale = meshRef.current.scale;
    
    // Convert to the format expected by the application
    const newPosition = {
      x: parseFloat(position.x.toFixed(3)),
      y: parseFloat(position.y.toFixed(3)),
      z: parseFloat(position.z.toFixed(3)),
      unit: object.position?.unit || 'cm'
    };
    
    // Convert rotation from radians to degrees
    const newRotation = {
      x: parseFloat((rotation.x * 180 / Math.PI).toFixed(3)),
      y: parseFloat((rotation.y * 180 / Math.PI).toFixed(3)),
      z: parseFloat((rotation.z * 180 / Math.PI).toFixed(3)),
      unit: object.rotation?.unit || 'deg'
    };
    
    // For real-time updates, we don't apply scale changes during dragging
    // We'll only update position and rotation
    onTransformEnd(objectKey, {
      position: newPosition,
      rotation: newRotation
    }, true);
    
    // Ensure object stays selected during transformation
    onSelect(objectKey);
  }, [isTransforming, meshRef, object, objectKey, onTransformEnd, onSelect]);
  
  // Handle transform end
  const handleTransformEnd = useCallback(() => {
    if (!isTransforming) return;
    
    // Only update the position when the transformation is complete
    if (meshRef.current) {
      const position = meshRef.current.position;
      const rotation = meshRef.current.rotation;
      const scale = meshRef.current.scale;
      
      // Convert to the format expected by the application
      const newPosition = {
        x: parseFloat(position.x.toFixed(3)),
        y: parseFloat(position.y.toFixed(3)),
        z: parseFloat(position.z.toFixed(3)),
        unit: object.position?.unit || 'cm'
      };
      
      // Convert rotation from radians to degrees
      const newRotation = {
        x: parseFloat((rotation.x * 180 / Math.PI).toFixed(3)),
        y: parseFloat((rotation.y * 180 / Math.PI).toFixed(3)),
        z: parseFloat((rotation.z * 180 / Math.PI).toFixed(3)),
        unit: object.rotation?.unit || 'deg'
      };
      
      // For box objects, we need to update the size based on scale
      let newSize;
      if (object.type === 'box' && object.size) {
        newSize = {
          x: parseFloat((object.size.x * scale.x).toFixed(3)),
          y: parseFloat((object.size.y * scale.y).toFixed(3)),
          z: parseFloat((object.size.z * scale.z).toFixed(3)),
          unit: object.size.unit || 'cm'
        };
      }
      
      // For cylinders and spheres, update radius and height
      let newRadius, newHeight;
      if (object.type === 'cylinder') {
        // For cylinders, x and z scale affect radius, y affects height
        // Since we rotated the cylinder to align with z-axis, we need to adjust which scale affects what
        newRadius = parseFloat((object.radius * ((scale.x + scale.z) / 2)).toFixed(3));
        newHeight = parseFloat((object.height * scale.y).toFixed(3));
      } else if (object.type === 'sphere') {
        // For spheres, use average scale for radius
        newRadius = parseFloat((object.radius * ((scale.x + scale.y + scale.z) / 3)).toFixed(3));
      }
      
      // Call the callback with the updated object
      onTransformEnd(objectKey, {
        position: newPosition,
        rotation: newRotation,
        ...(newSize && { size: newSize }),
        ...(newRadius !== undefined && { radius: newRadius }),
        ...(newHeight !== undefined && { height: newHeight })
      }, true);
      
      // Reset scale after applying it to the dimensions
      meshRef.current.scale.set(1, 1, 1);
    }
    
    // Re-enable orbit controls
    const controls = gl.domElement.parentElement?.__r3f?.controls;
    if (controls) {
      controls.enabled = true;
    }
    
    gl.domElement.style.cursor = 'auto';
    setIsTransforming(false);
    
    // Force selection to stay on this object after transformation
    onSelect(objectKey);
  }, [isTransforming, meshRef, object, objectKey, onTransformEnd, gl, onSelect]);
  
  return (
    <group>
      {/* Render the appropriate mesh first */}
      {object.type === 'box' && (
        <BoxObject
          ref={meshRef}
          object={object}
          isSelected={isSelected}
          onClick={() => onSelect(objectKey)}
        />
      )}
      {object.type === 'cylinder' && (
        <CylinderObject
          ref={meshRef}
          object={object}
          isSelected={isSelected}
          onClick={() => onSelect(objectKey)}
        />
      )}
      {object.type === 'sphere' && (
        <SphereObject
          ref={meshRef}
          object={object}
          isSelected={isSelected}
          onClick={() => onSelect(objectKey)}
        />
      )}
      
      {/* Add transform controls after the mesh is rendered, but not for World */}
      {isSelected && meshRef.current && !isWorld && (
        <CustomTransformControls
          object={meshRef}
          mode={transformMode}
          enabled={isSelected}
          onTransformStart={handleTransformStart}
          onTransformChange={handleTransformChange}
          onTransformEnd={handleTransformEnd}
        />
      )}
    </group>
  );
}

// Box Object Component
const BoxObject = React.forwardRef(({ object, isSelected, onClick }, ref) => {
  const position = object.position ? [
    object.position.x, 
    object.position.y, 
    object.position.z
  ] : [0, 0, 0];
  
  const size = object.size ? [
    object.size.x, 
    object.size.y, 
    object.size.z
  ] : [10, 10, 10];
  
  const rotation = object.rotation ? [
    object.rotation.x * Math.PI / 180,
    object.rotation.y * Math.PI / 180,
    object.rotation.z * Math.PI / 180
  ] : [0, 0, 0];
  
  // Check if this is the World volume
  const isWorld = object.name === 'World';
  
  return (
    <mesh 
      ref={ref}
      position={position}
      rotation={rotation}
      onClick={(e) => {
        e.stopPropagation();
        if (onClick) onClick();
      }}
    >
      <boxGeometry args={size} />
      {isWorld ? (
        // World volume - transparent wireframe only
        <meshStandardMaterial 
          color="rgba(200, 200, 255, 0.3)" 
          wireframe={true}
          transparent={true}
          opacity={0.5}
        />
      ) : (
        // Regular box volume
        <meshStandardMaterial 
          color="rgba(100, 100, 255, 0.7)" 
          transparent={true}
          opacity={0.7}
        />
      )}
      {isSelected && (
        <lineSegments>
          <edgesGeometry attach="geometry" args={[new THREE.BoxGeometry(...size)]} />
          <lineBasicMaterial attach="material" color="#ffff00" />
        </lineSegments>
      )}
    </mesh>
  );
});

// Cylinder Object Component
const CylinderObject = React.forwardRef(({ object, isSelected, onClick }, ref) => {
  const position = object.position ? [
    object.position.x, 
    object.position.y, 
    object.position.z
  ] : [0, 0, 0];
  
  const radius = object.radius || 5;
  const height = object.height || 10;
  
  // Convert rotation from degrees to radians
  const rotation = object.rotation ? [
    object.rotation.x * Math.PI / 180,
    object.rotation.y * Math.PI / 180,
    object.rotation.z * Math.PI / 180
  ] : [0, 0, 0];
  
  return (
    <mesh 
      ref={ref}
      position={position}
      rotation={rotation}
      onClick={(e) => {
        e.stopPropagation();
        if (onClick) onClick();
      }}
    >
      <cylinderGeometry args={[radius, radius, height, 32]} />
      <meshStandardMaterial 
        color="rgba(100, 255, 100, 0.7)" 
        transparent={true}
        opacity={0.7}
      />
      {isSelected && (
        <lineSegments>
          <edgesGeometry attach="geometry" args={[new THREE.CylinderGeometry(radius, radius, height, 32)]} />
          <lineBasicMaterial attach="material" color="#ffff00" />
        </lineSegments>
      )}
    </mesh>
  );
});

// Sphere Object Component
const SphereObject = React.forwardRef(({ object, isSelected, onClick }, ref) => {
  const position = object.position ? [
    object.position.x, 
    object.position.y, 
    object.position.z
  ] : [0, 0, 0];
  
  const radius = object.radius || 5;
  
  const rotation = object.rotation ? [
    object.rotation.x * Math.PI / 180,
    object.rotation.y * Math.PI / 180,
    object.rotation.z * Math.PI / 180
  ] : [0, 0, 0];
  
  return (
    <mesh 
      ref={ref}
      position={position}
      rotation={rotation}
      onClick={(e) => {
        e.stopPropagation();
        if (onClick) onClick();
      }}
    >
      <sphereGeometry args={[radius, 32, 32]} />
      <meshStandardMaterial 
        color="rgba(255, 100, 100, 0.7)" 
        transparent={true}
        opacity={0.7}
      />
      {isSelected && (
        <lineSegments>
          <edgesGeometry attach="geometry" args={[new THREE.SphereGeometry(radius, 32, 32)]} />
          <lineBasicMaterial attach="material" color="#ffff00" />
        </lineSegments>
      )}
    </mesh>
  );
});

// Simple Scene component
function Scene({ geometries, selectedGeometry, onSelect, setFrontViewCamera, transformMode, onTransformEnd }) {
  // World box
  const worldSize = geometries.world.size ? [
    geometries.world.size.x, 
    geometries.world.size.y, 
    geometries.world.size.z
  ] : [100, 100, 100];
  
  const worldPosition = geometries.world.position ? [
    geometries.world.position.x, 
    geometries.world.position.y, 
    geometries.world.position.z
  ] : [0, 0, 0];

  return (
    <>
      <ambientLight intensity={0.5} />
      <directionalLight position={[10, 10, 10]} intensity={1} />
      <CoordinateSystem />
      <CameraSetup setFrontViewCamera={setFrontViewCamera} />
      
      {/* World volume */}
      <TransformableObject 
        object={geometries.world}
        objectKey="world"
        isSelected={selectedGeometry === 'world'}
        transformMode={transformMode}
        onSelect={onSelect}
        onTransformEnd={onTransformEnd}
      />
      
      {/* Volume objects */}
      {geometries.volumes && geometries.volumes.map((volume, index) => {
        const key = `volume-${index}`;
        return (
          <TransformableObject 
            key={key}
            object={volume}
            objectKey={key}
            isSelected={selectedGeometry === key}
            transformMode={transformMode}
            onSelect={onSelect}
            onTransformEnd={onTransformEnd}
          />
        );
      })}
    </>
  );
}

// GeometryTree component for the left panel
const GeometryTree = ({ geometries, selectedGeometry, onSelect }) => {
  return (
    <div style={{ padding: '10px', backgroundColor: '#f5f5f5', height: '100%', overflowY: 'auto' }}>
      <h3 style={{ margin: '0 0 10px 0' }}>Geometry Tree</h3>
      <div style={{ marginBottom: '5px' }}>
        <div 
          onClick={() => onSelect('world')}
          style={{
            padding: '8px',
            backgroundColor: selectedGeometry === 'world' ? '#1976d2' : '#fff',
            color: selectedGeometry === 'world' ? '#fff' : '#000',
            borderRadius: '4px',
            cursor: 'pointer',
            marginBottom: '5px',
            display: 'flex',
            alignItems: 'center'
          }}
        >
          <span style={{ marginRight: '5px' }}>🌐</span>
          <strong>World</strong>
        </div>
        
        {geometries.volumes && geometries.volumes.map((volume, index) => {
          const key = `volume-${index}`;
          let icon = '📦';
          if (volume.type === 'sphere') icon = '��';
          if (volume.type === 'cylinder') icon = '🧪';
          
          return (
            <div 
              key={key}
              onClick={() => onSelect(key)}
              style={{
                padding: '8px',
                backgroundColor: selectedGeometry === key ? '#1976d2' : '#fff',
                color: selectedGeometry === key ? '#fff' : '#000',
                borderRadius: '4px',
                cursor: 'pointer',
                marginBottom: '5px',
                marginLeft: '15px',
                display: 'flex',
                alignItems: 'center'
              }}
            >
              <span style={{ marginRight: '5px' }}>{icon}</span>
              {volume.name || `${volume.type.charAt(0).toUpperCase() + volume.type.slice(1)} ${index + 1}`}
            </div>
          );
        })}
      </div>
    </div>
  );
};

const Viewer3D = ({ geometries, selectedGeometry, onSelect, onUpdateGeometry }) => {
  const [transformMode, setTransformMode] = useState('translate');
  const [frontViewCamera, setFrontViewCamera] = useState(null);
  
  // Function to set front view
  const setFrontView = () => {
    if (frontViewCamera) {
      frontViewCamera.position.set(0, 0, 250);
      frontViewCamera.lookAt(0, 0, 0);
      frontViewCamera.updateProjectionMatrix();
    }
  };
  
  // Handle canvas click to deselect
  const handleCanvasClick = (e) => {
    if (selectedGeometry && e.target === e.currentTarget) {
      onSelect(null);
    }
  };
  
  // Handle transform end
  const handleTransformEnd = (objectKey, updates, keepSelected = true) => {
    // Get the current object
    let currentObject;
    if (objectKey === 'world') {
      currentObject = { ...geometries.world };
    } else if (objectKey.startsWith('volume-')) {
      const index = parseInt(objectKey.split('-')[1]);
      currentObject = { ...geometries.volumes[index] };
    } else {
      return;
    }
    
    // Apply updates to the current object
    const updatedObject = { ...currentObject };
    
    // Update position
    if (updates.position) {
      updatedObject.position = {
        ...updatedObject.position,
        ...updates.position
      };
    }
    
    // Update rotation
    if (updates.rotation) {
      updatedObject.rotation = {
        ...updatedObject.rotation,
        ...updates.rotation
      };
    }
    
    // Update size for box
    if (updates.size && updatedObject.type === 'box') {
      updatedObject.size = {
        ...updatedObject.size,
        ...updates.size
      };
    }
    
    // Update radius and height for cylinder
    if (updatedObject.type === 'cylinder') {
      if (updates.radius !== undefined) {
        updatedObject.radius = updates.radius;
      }
      if (updates.height !== undefined) {
        updatedObject.height = updates.height;
      }
    }
    
    // Update radius for sphere
    if (updatedObject.type === 'sphere' && updates.radius !== undefined) {
      updatedObject.radius = updates.radius;
    }
    
    // Call the update function with the keepSelected parameter
    onUpdateGeometry(objectKey, updatedObject, keepSelected);
  };
  
  // Check if geometries is valid
  if (!geometries || !geometries.world) {
    return <div>Loading geometries...</div>;
  }
  
  return (
    <div style={{ width: '100%', height: '100%', position: 'relative', display: 'flex' }}>
      {/* Left panel with geometry tree */}
      <div style={{ width: '250px', height: '100%', borderRight: '1px solid #ddd' }}>
        <GeometryTree 
          geometries={geometries} 
          selectedGeometry={selectedGeometry} 
          onSelect={onSelect} 
        />
      </div>
      
      {/* 3D Viewer */}
      <div style={{ flex: 1, height: '100%', position: 'relative' }}>
        {/* Transform mode buttons - top left */}
        <div style={{
          position: 'absolute',
          top: '10px',
          left: '10px',
          zIndex: 100,
          display: 'flex',
          gap: '5px'
        }}>
          <button 
            onClick={() => setTransformMode('translate')}
            style={{
              backgroundColor: transformMode === 'translate' ? '#1976d2' : '#f1f1f1',
              color: transformMode === 'translate' ? 'white' : 'black',
              border: 'none',
              padding: '5px 10px',
              borderRadius: '4px',
              cursor: 'pointer'
            }}
          >
            Move
          </button>
          <button 
            onClick={() => setTransformMode('rotate')}
            style={{
              backgroundColor: transformMode === 'rotate' ? '#1976d2' : '#f1f1f1',
              color: transformMode === 'rotate' ? 'white' : 'black',
              border: 'none',
              padding: '5px 10px',
              borderRadius: '4px',
              cursor: 'pointer'
            }}
          >
            Rotate
          </button>
          <button 
            onClick={() => setTransformMode('scale')}
            style={{
              backgroundColor: transformMode === 'scale' ? '#1976d2' : '#f1f1f1',
              color: transformMode === 'scale' ? 'white' : 'black',
              border: 'none',
              padding: '5px 10px',
              borderRadius: '4px',
              cursor: 'pointer'
            }}
          >
            Scale
          </button>
        </div>
        
        {/* Front view button - bottom left */}
        <div style={{
          position: 'absolute',
          bottom: '10px',
          left: '10px',
          zIndex: 100
        }}>
          <button 
            onClick={setFrontView}
            style={{
              backgroundColor: '#1976d2',
              color: 'white',
              border: 'none',
              padding: '5px 10px',
              borderRadius: '4px',
              cursor: 'pointer'
            }}
          >
            Front View
          </button>
        </div>
        
        <Canvas 
          style={{ background: '#f0f0f0' }} 
          onClick={handleCanvasClick}
        >
          <Scene 
            geometries={geometries} 
            selectedGeometry={selectedGeometry} 
            onSelect={onSelect}
            setFrontViewCamera={setFrontViewCamera}
            transformMode={transformMode}
            onTransformEnd={handleTransformEnd}
          />
          <OrbitControls makeDefault enableDamping={false} />
        </Canvas>
      </div>
    </div>
  );
};

export default Viewer3D;
