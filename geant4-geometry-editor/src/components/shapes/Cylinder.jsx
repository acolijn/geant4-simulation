import React, { useState, forwardRef, useImperativeHandle } from 'react';
import { useFrame } from '@react-three/fiber';
import * as THREE from 'three';

const Cylinder = forwardRef(({ position, radius, height, innerRadius = 0, rotation, color, wireframe, selected, onClick }, ref) => {
  const mesh = React.useRef();
  
  // Forward the mesh ref to parent
  useImperativeHandle(ref, () => mesh.current);
  const [hovered, setHovered] = useState(false);
  
  // Highlight effect when selected or hovered
  useFrame(() => {
    if (mesh.current && mesh.current.material) {
      if (selected) {
        mesh.current.material.emissive = new THREE.Color(0x555555);
      } else if (hovered) {
        mesh.current.material.emissive = new THREE.Color(0x333333);
      } else {
        mesh.current.material.emissive = new THREE.Color(0x000000);
      }
    }
  });

  // For Geant4, cylinders are typically along the z-axis
  // We need to rotate them to match this convention
  const defaultRotation = [Math.PI / 2, 0, 0];
  const finalRotation = rotation ? [
    defaultRotation[0] + rotation[0],
    defaultRotation[1] + rotation[1],
    defaultRotation[2] + rotation[2]
  ] : defaultRotation;

  return (
    <mesh
      ref={mesh}
      position={position}
      rotation={finalRotation}
      onClick={(e) => {
        e.stopPropagation();
        if (onClick) onClick();
      }}
      onPointerDown={(e) => {
        e.stopPropagation();
      }}
      onPointerOver={() => setHovered(true)}
      onPointerOut={() => setHovered(false)}
      castShadow
      receiveShadow
    >
      <cylinderGeometry args={[radius, radius, height, 32, 1, innerRadius > 0, innerRadius]} />
      <meshStandardMaterial 
        color={color || 'rgba(100, 255, 100, 0.7)'} 
        wireframe={wireframe}
        transparent={true}
        opacity={wireframe ? 0.3 : 0.7}
      />
      {selected && (
        <lineSegments>
          <edgesGeometry attach="geometry" args={[new THREE.CylinderGeometry(radius, radius, height, 32, 1, innerRadius > 0, innerRadius)]} />
          <lineBasicMaterial attach="material" color="#ffff00" />
        </lineSegments>
      )}
    </mesh>
  );
});

export default Cylinder;
