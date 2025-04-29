import React, { useState, forwardRef, useImperativeHandle } from 'react';
import { useFrame } from '@react-three/fiber';
import * as THREE from 'three';

const Box = forwardRef(({ position, size, rotation, color, wireframe, selected, onClick }, ref) => {
  const mesh = React.useRef();
  const [hovered, setHovered] = useState(false);
  
  // Forward the mesh ref to parent
  useImperativeHandle(ref, () => mesh.current);
  
  // Ensure we have valid position, size, and rotation
  const safePosition = position || [0, 0, 0];
  const safeSize = size || [1, 1, 1];
  const safeRotation = rotation || [0, 0, 0];
  
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
      rotation={safeRotation}
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
      <boxGeometry args={safeSize} />
      <meshStandardMaterial 
        color={color || 'rgba(100, 100, 255, 0.7)'} 
        wireframe={wireframe}
        transparent={true}
        opacity={wireframe ? 0.3 : 0.7}
      />
      {selected && (
        <lineSegments>
          <edgesGeometry attach="geometry" args={[new THREE.BoxGeometry(...safeSize)]} />
          <lineBasicMaterial attach="material" color="#ffff00" />
        </lineSegments>
      )}
    </mesh>
  );
});

export default Box;
