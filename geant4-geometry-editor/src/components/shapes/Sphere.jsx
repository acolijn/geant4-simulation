import React, { useState, forwardRef, useImperativeHandle } from 'react';
import { useFrame } from '@react-three/fiber';
import * as THREE from 'three';

const Sphere = forwardRef(({ position, radius, color, wireframe, selected, onClick }, ref) => {
  const mesh = React.useRef();
  
  // Forward the mesh ref to parent
  useImperativeHandle(ref, () => mesh.current);
  const [hovered, setHovered] = useState(false);
  
  // Ensure we have valid position and radius
  const safePosition = position || [0, 0, 0];
  const safeRadius = radius || 1;
  
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

  return (
    <mesh
      ref={mesh}
      position={safePosition}
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
      <sphereGeometry args={[safeRadius, 32, 32]} />
      <meshStandardMaterial 
        color={color || 'rgba(255, 100, 100, 0.7)'} 
        wireframe={wireframe}
        transparent={true}
        opacity={wireframe ? 0.3 : 0.7}
      />
      {selected && (
        <lineSegments>
          <edgesGeometry attach="geometry" args={[new THREE.SphereGeometry(safeRadius, 32, 32)]} />
          <lineBasicMaterial attach="material" color="#ffff00" />
        </lineSegments>
      )}
    </mesh>
  );
});

export default Sphere;
