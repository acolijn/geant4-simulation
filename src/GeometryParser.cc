#include "GeometryParser.hh"
#include "G4NistManager.hh"
#include "G4SDManager.hh"
#include "MySensitiveDetector.hh"

// Basic shapes
#include "G4Box.hh"
#include "G4Sphere.hh"
#include "G4Tubs.hh"
#include "G4Cons.hh"
#include "G4Trd.hh"
#include "G4Trap.hh"
#include "G4Para.hh"
#include "G4Torus.hh"
#include "G4Polycone.hh"
#include "G4Polyhedra.hh"
#include "G4Ellipsoid.hh"
#include "G4EllipticalTube.hh"
#include "G4Orb.hh"

// Boolean operations
#include "G4UnionSolid.hh"
#include "G4SubtractionSolid.hh"
#include "G4IntersectionSolid.hh"

// Placement
#include "G4PVPlacement.hh"
#include "G4PVParameterised.hh"
#include "G4AssemblyVolume.hh"
#include "G4VisAttributes.hh"

#include "G4SystemOfUnits.hh"
#include "G4PhysicalConstants.hh"
#include <fstream>
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

/**
 * @brief Default constructor for GeometryParser
 * @details Initializes an empty geometry parser without loading any configuration
 */
GeometryParser::GeometryParser()
{}

/**
 * @brief Destructor for GeometryParser
 * @details Memory management of G4 objects is handled by Geant4
 */
GeometryParser::~GeometryParser()
{}

/**
 * @brief Load detector geometry configuration from a JSON file
 * @param filename Path to the JSON configuration file
 * @throws std::runtime_error if file cannot be opened or parsed
 * @details Reads and parses the geometry configuration file into the geometryConfig member
 *          and stores the config path for loading external files
 */
void GeometryParser::LoadGeometryConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open geometry config file: " + filename);
    }
    file >> geometryConfig;
    
    // Store the directory path for loading external files
    configPath = fs::path(filename).parent_path().string();
}

/**
 * @brief Load material definitions from a JSON file
 * @param filename Path to the JSON materials configuration file
 * @throws std::runtime_error if file cannot be opened or parsed
 * @details Reads and parses the materials configuration file into the materialsConfig member
 */
/*void GeometryParser::LoadMaterialsConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open materials config file: " + filename);
    }
    file >> materialsConfig;
} */

/**
 * @brief Create a G4Material from JSON configuration
 * @param name Name of the material to create
 * @param config JSON object containing material properties
 * @return Pointer to the created or cached G4Material
 * @throws std::runtime_error if material creation fails
 * @details Supports creation of materials from NIST database or by element composition.
 *          For compound materials, requires density, state, temperature, and composition.
 */
G4Material* GeometryParser::CreateMaterial(const std::string& name, const json& config) {
    // Check if material already exists
    if (materials.find(name) != materials.end()) {
        return materials[name];
    }

    // print material config
    std::cout << "Material config for '" << name << "': " << config << std::endl;
    
    G4Material* material = nullptr;
    std::string type = config["type"].get<std::string>();

    std::cout << "Material type: " << type << std::endl;
    if (type == "nist") {
        // Get material from NIST database
        // For NIST materials, the name itself is the material name (e.g., G4_WATER)
        std::cout << "NIST material: " << name << std::endl;
        G4NistManager* nist = G4NistManager::Instance();
        material = nist->FindOrBuildMaterial(name);
    }
    else if (type == "element_based" || type == "compound") {
        // Get basic properties
        G4double density = config["density"].get<double>();
        std::string density_unit = config["density_unit"].get<std::string>();
        if (density_unit == "g/cm3") density *= g/cm3;
        
        G4State state = kStateUndefined;
        std::string state_str = config["state"].get<std::string>();
        if (state_str == "solid") state = kStateSolid;
        else if (state_str == "liquid") state = kStateLiquid;
        else if (state_str == "gas") state = kStateGas;

        G4double temperature = config["temperature"].get<double>();
        std::string temp_unit = config["temperature_unit"].get<std::string>();
        if (temp_unit == "kelvin") temperature *= kelvin;

        // Create the material
        material = new G4Material(name, density, 
                                config["composition"].size(), 
                                state, temperature);

        // Add elements
        G4NistManager* nist = G4NistManager::Instance();
        for (auto& [element_name, count] : config["composition"].items()) {
            G4Element* element = nist->FindOrBuildElement(element_name);
            material->AddElement(element, count.get<int>());
        }
    } else {
        throw std::runtime_error("Invalid material type: " + type);
    }

    if (!material) {
        throw std::runtime_error("Failed to create material: " + name);
    }

    std::cout << "Created material: " << name << std::endl;
    materials[name] = material;
    return material;
}

/**
 * @brief Convert a JSON vector definition to G4ThreeVector
 * @param vec JSON object containing x,y,z components
 * @return G4ThreeVector with components in mm
 * @details Expects JSON format: {"x": val, "y": val, "z": val}
 *          All values are assumed to be in mm
 */
G4ThreeVector GeometryParser::ParseVector(const json& vec) {
    G4double x = vec["x"].get<double>();
    G4double y = vec["y"].get<double>();
    G4double z = vec["z"].get<double>();
    
    // All values are assumed to be in mm, so we multiply by mm to get the correct Geant4 units
    return G4ThreeVector(x*mm, y*mm, z*mm);
}

/**
 * @brief Convert JSON rotation angles to G4RotationMatrix
 * @param rot JSON object containing rotation angles
 * @return Pointer to new G4RotationMatrix
 * @details Expects JSON format: {"x": angle_x, "y": angle_y, "z": angle_z}
 *          Angles are assumed to be in radians
 *          Rotations are applied in the Geant4 sequence: first X, then Y, then Z
 */
G4RotationMatrix* GeometryParser::ParseRotation(const json& rot) {
    G4double rx = rot["x"].get<double>();
    G4double ry = rot["y"].get<double>();
    G4double rz = rot["z"].get<double>();
    
    // Create a rotation matrix using individual rotations around each axis
    // This ensures rotations are applied in the correct sequence: X, then Y, then Z
    G4RotationMatrix* rotMatrix = new G4RotationMatrix();
    
    // Apply rotations in sequence (Geant4 uses active rotations)
    // Values are already in radians, but we need to multiply by rad for Geant4 units
    rotMatrix->rotateX(rx*rad);
    rotMatrix->rotateY(ry*rad);
    rotMatrix->rotateZ(rz*rad);
    
    return rotMatrix;
}

/**
 * @brief Parse a placement object and extract position and rotation
 * @param placement JSON object containing placement information
 * @param position Reference to G4ThreeVector to store position
 * @param rotation Reference to G4RotationMatrix pointer to store rotation
 * @details Handles the geometry-editor format with placement object
 */
void GeometryParser::ParsePlacement(const json& placement, G4ThreeVector& position, G4RotationMatrix*& rotation) {
    // Initialize with default values
    position = G4ThreeVector(0, 0, 0);
    rotation = nullptr;

    // Parse position
    if (placement.contains("x") && placement.contains("y") && placement.contains("z")) {
        G4double x = placement["x"].get<double>();
        G4double y = placement["y"].get<double>();
        G4double z = placement["z"].get<double>();
        position = G4ThreeVector(x*mm, y*mm, z*mm);
    }

    // Parse rotation
    if (placement.contains("rotation")) {
        rotation = ParseRotation(placement["rotation"]);
    }
}

/**
 * @brief Create a G4LogicalVolume from JSON configuration
 * @param config JSON object containing volume properties
 * @return Pointer to the created G4LogicalVolume
 * @throws std::runtime_error if volume creation fails
 * @details Creates a logical volume with specified material and shape.
 *          Supports all Geant4 shapes, boolean operations, and external geometries.
 */
G4LogicalVolume* GeometryParser::CreateVolume(const json& config) {
    // Debug output to check config structure
    G4cout << "GeometryParser::CreateVolume() - Config keys available:";
    for (auto it = config.begin(); it != config.end(); ++it) {
        G4cout << " " << it.key();
    }
    G4cout << G4endl;
    
    // Check if required keys exist
    std::string g4name;
    if (config.contains("g4name")) {
        g4name = config["g4name"].get<std::string>();
        G4cout << "GeometryParser::CreateVolume() - Creating volume: " << g4name << G4endl;
    } else {
        // If g4name is missing, use name if available or generate a default
        if (config.contains("name")) {
            g4name = config["name"].get<std::string>() + "_g4";
            G4cout << "GeometryParser::CreateVolume() - g4name missing, using derived name: " << g4name << G4endl;
        } else {
            g4name = "UnnamedVolume_" + std::to_string(volumes.size());
            G4cout << "GeometryParser::CreateVolume() - Both g4name and name missing, using generated name: " << g4name << G4endl;
        }
    }
    
    if (!config.contains("name")) {
        G4cerr << "GeometryParser::CreateVolume() - Error: name key not found in config" << G4endl;
        throw std::runtime_error("Missing name key in volume config");
    }
    std::string name = config["name"].get<std::string>();
    
    if (!config.contains("type")) {
        G4cerr << "GeometryParser::CreateVolume() - Error: type key not found in config" << G4endl;
        throw std::runtime_error("Missing type key in volume config");
    }
    std::string type = config["type"].get<std::string>();
    
    // Check if volume already exists
    if (volumes.find(name) != volumes.end()) {
        return volumes[name];
    }

    // Get material
    G4Material* material = nullptr;
    
    // Check if material key exists in the config
    if (config.contains("material")) {
        G4cout << "GeometryParser::CreateVolume() - Material: " << config["material"] << G4endl;
        std::string mat_name = config["material"].get<std::string>();
        
        if (materials.find(mat_name) == materials.end()) {
            material = CreateMaterial(mat_name, geometryConfig["materials"][mat_name]);
//
//            material = CreateMaterial(mat_name, materialsConfig["materials"][mat_name]);
        } else {
            material = materials[mat_name];
        }
        G4cout << "GeometryParser::CreateVolume() - Material: " << material << G4endl;
    } else {
        // Default to G4_AIR if no material is specified
        G4cout << "GeometryParser::CreateVolume() - No material specified, using G4_AIR" << G4endl;
        G4NistManager* nistManager = G4NistManager::Instance();
        material = nistManager->FindOrBuildMaterial("G4_AIR");
    }

    // Special handling for union volumes with components
    G4cout << "GeometryParser::CreateVolume() - Creating solid: " << name << G4endl;
    G4VSolid* solid = CreateSolid(config, name);
    G4cout << "GeometryParser::CreateVolume() - Created solid: " << solid << G4endl;
    // Create logical volume
    G4LogicalVolume* logicalVolume = new G4LogicalVolume(solid, material, name);
    volumes[name] = logicalVolume;
    
    std::cout << "GeometryParser::CreateVolume() - Created volume: " << name << std::endl;
    return logicalVolume;
}

/**
 * @brief Construct the complete detector geometry
 * @return Pointer to the world physical volume
 * @details Creates the world volume and places all other volumes within it
 *          according to the loaded geometry configuration from the geometry-editor
 */
G4VPhysicalVolume* GeometryParser::ConstructGeometry() {
    // Create world volume
    G4cout << "GeometryParser::ConstructGeometry() - Creating world volume" << G4endl;
    G4LogicalVolume* worldLV = CreateVolume(geometryConfig["world"]);    
    auto worldVis = new G4VisAttributes();
    worldVis->SetVisibility(false);
    worldLV->SetVisAttributes(worldVis);
    G4VPhysicalVolume* worldPV = new G4PVPlacement(
        nullptr, G4ThreeVector(), worldLV, "World", nullptr, false, 0);
    G4cout << "GeometryParser::ConstructGeometry() - Created world physical volume" << G4endl;
    
    // Store the world volume in our volumes map for parent references
    std::string worldName = geometryConfig["world"]["name"].get<std::string>();
    volumes[worldName] = worldLV;
    
    // First pass: Create all logical volumes (skip assemblies)
    G4LogicalVolume* logicalVolume;
    G4cout << "GeometryParser::ConstructGeometry() - First pass: Create all logical volumes (skip assemblies)" << G4endl;
    
    // Debug the volumes array structure
    G4cout << "GeometryParser::ConstructGeometry() - Number of volumes in config: " << geometryConfig["volumes"].size() << G4endl;
    
    for (size_t i = 0; i < geometryConfig["volumes"].size(); i++) {
        const auto& volConfig = geometryConfig["volumes"][i];
        
        // Check if required keys exist
        if (!volConfig.contains("type")) {
            G4cerr << "Error: type key not found in volume " << i << G4endl;
            continue;
        }
        
        if (volConfig["type"].get<std::string>() == "assembly") {
            G4cout << "GeometryParser::ConstructGeometry() - Skipping assembly volume" << G4endl;
            continue;
        }
        
        if (!volConfig.contains("name")) {
            G4cerr << "Error: name key not found in volume " << i << G4endl;
            continue;
        }
        
        G4cout << "GeometryParser::ConstructGeometry() - Creating logical volume: " << volConfig["name"].get<std::string>() << G4endl;
        
        try {
            logicalVolume = CreateVolume(volConfig);
            std::string name = volConfig["name"].get<std::string>();
            volumes[name] = logicalVolume;
            G4cout << "GeometryParser::ConstructGeometry() - Created logical volume: " << name << G4endl;
        } catch (const std::exception& e) {
            G4cerr << "GeometryParser::ConstructGeometry() - Error creating volume: " << e.what() << G4endl;
        }
    }

    // Create all assemblies before placement
    for (const auto& volConfig : geometryConfig["volumes"]) {
        if (volConfig["type"].get<std::string>() == "assembly") {
            CreateAssembly(volConfig);
        }
    }
    
    // Create a map to track which volumes have been placed
    std::set<std::string> placedVolumes;
    placedVolumes.insert("World"); // World is already placed
    
    // Second pass: First place volumes with World as parent
    for (const auto& volConfig : geometryConfig["volumes"]) {
        if (volConfig["type"].get<std::string>() == "assembly") continue;
        std::string name = volConfig["name"].get<std::string>();        
        G4LogicalVolume* logicalVolume = volumes[name];
        
        // Skip if no placements array
        if (!volConfig.contains("placements") || volConfig["placements"].empty()) {
            G4cout << "GeometryParser::ConstructGeometry() - Warning: No placements for volume " << name << G4endl;
            continue;
        }
        
        // Process each placement - only those with World as parent in this pass
        for (const auto& placement : volConfig["placements"]) {
            // Get parent volume
            std::string parentName = "World"; // Default to world if no parent specified
            if (placement.contains("parent")) {
                parentName = placement["parent"].get<std::string>();
            }
            
            // Only process volumes with World as parent in this pass
            if (parentName != "World") continue;
            
            // Get position and rotation
            G4ThreeVector position;
            G4RotationMatrix* rotation = nullptr;
            ParsePlacement(placement, position, rotation);
            
            // Check if parent exists
            if (volumes.find(parentName) == volumes.end()) {
                G4cerr << "Error: Parent volume " << parentName << " not found for " << name << G4endl;
                continue;
            }
            
            G4LogicalVolume* parentVolume = volumes[parentName];
            
            // Place the volume
            G4cout << "GeometryParser::ConstructGeometry() - Placing " << name << " in " << parentName 
                   << " at position " << position << G4endl;
            
            // Use g4name if available, otherwise use name
            std::string physicalName = volConfig.contains("g4name") ? 
                                      volConfig["g4name"].get<std::string>() : 
                                      name;
            
            new G4PVPlacement(
                rotation,           // rotation
                position,           // position
                logicalVolume,      // logical volume
                physicalName,       // name
                parentVolume,       // mother volume
                false,              // no boolean operations
                0                   // copy number
            );
            
            // Mark this volume as placed
            placedVolumes.insert(name);
        }
    }
    
    // Now place volumes with non-World parents in a way that respects the hierarchy
    bool placedAny = true;
    while (placedAny) {
        placedAny = false;
        
        for (const auto& volConfig : geometryConfig["volumes"]) {
            if (volConfig["type"].get<std::string>() == "assembly") continue;
            std::string name = volConfig["name"].get<std::string>();
            
            // Skip if already placed
            if (placedVolumes.find(name) != placedVolumes.end()) continue;
            
            G4LogicalVolume* logicalVolume = volumes[name];
            
            // Skip if no placements array
            if (!volConfig.contains("placements") || volConfig["placements"].empty()) continue;
            
            // Process each placement
            for (const auto& placement : volConfig["placements"]) {
                // Get parent volume
                std::string parentName = "World"; // Default to world if no parent specified
                if (placement.contains("parent")) {
                    parentName = placement["parent"].get<std::string>();
                }
                
                // Skip if parent hasn't been placed yet
                if (placedVolumes.find(parentName) == placedVolumes.end()) continue;
                
                // Get position and rotation
                G4ThreeVector position;
                G4RotationMatrix* rotation = nullptr;
                ParsePlacement(placement, position, rotation);
                
                // Check if parent exists
                if (volumes.find(parentName) == volumes.end()) {
                    G4cerr << "Error: Parent volume " << parentName << " not found for " << name << G4endl;
                    continue;
                }
                
                G4LogicalVolume* parentVolume = volumes[parentName];
                
                // Place the volume
                G4cout << "GeometryParser::ConstructGeometry() - Placing " << name << " in " << parentName 
                       << " at position " << position << G4endl;
                
                // Use g4name if available, otherwise use name
                std::string physicalName = volConfig.contains("g4name") ? 
                                          volConfig["g4name"].get<std::string>() : 
                                          name;
                
                new G4PVPlacement(
                    rotation,           // rotation
                    position,           // position
                    logicalVolume,      // logical volume
                    physicalName,       // name
                    parentVolume,       // mother volume
                    false,              // no boolean operations
                    0                   // copy number
                );
                
                // Mark this volume as placed
                placedVolumes.insert(name);
                placedAny = true;
                break; // We've placed this volume, move on to the next one
            }
        }
    }
    
    // Check if any volumes weren't placed (could indicate circular dependencies)
    for (const auto& volConfig : geometryConfig["volumes"]) {
        if (volConfig["type"].get<std::string>() == "assembly") continue;
        std::string name = volConfig["name"].get<std::string>();
        if (placedVolumes.find(name) == placedVolumes.end()) {
            G4cerr << "Warning: Volume " << name << " could not be placed. Check for circular dependencies." << G4endl;
        }
    }

    // Place all assemblies
    for (const auto& volConfig : geometryConfig["volumes"]) {
        // Skip non-assembly volumes
        if (volConfig["type"].get<std::string>() != "assembly") {
            G4cout << "GeometryParser::ConstructGeometry() - Skipping non-assembly volume" << G4endl;
            continue;
        }
        
        // Get the assembly name
        std::string assemblyName = volConfig["name"].get<std::string>();
        G4AssemblyVolume* assembly = assemblies[assemblyName];
        
        if (!assembly) {
            G4cerr << "Error: Assembly " << assemblyName << " not found" << G4endl;
            continue;
        }
        
        // Skip if no placements array
        if (!volConfig.contains("placements") || volConfig["placements"].empty()) {
            G4cout << "GeometryParser::ConstructGeometry() - Warning: No placements for assembly " << assemblyName << G4endl;
            continue;
        }
        
        // Process each placement
        int iCopy = 0;
        for (const auto& placement : volConfig["placements"]) {
            // Get position
            G4double x = placement["x"].get<double>();
            G4double y = placement["y"].get<double>();
            G4double z = placement["z"].get<double>();
            G4ThreeVector position(x*mm, y*mm, z*mm);
            
            // Get rotation if present
            G4RotationMatrix* rotation = nullptr;
            if (placement.contains("rotation")) {
                rotation = ParseRotation(placement["rotation"]);
            }
            
            // Get parent volume
            std::string parentName = "World"; // Default to world if no parent specified
            if (placement.contains("parent")) {
                parentName = placement["parent"].get<std::string>();
            }
            
            // Check if parent exists
            if (volumes.find(parentName) == volumes.end()) {
                G4cerr << "Error: Parent volume " << parentName << " not found for assembly " << assemblyName << G4endl;
                continue;
            }
            
            G4LogicalVolume* parentVolume = volumes[parentName];
            
            // Place the assembly
            G4cout << "GeometryParser::ConstructGeometry() - Placing assembly " << assemblyName << " in " << parentName 
                   << " at position " << position << G4endl;
            
            assembly->MakeImprint(parentVolume, position, rotation, iCopy++, true);
        }
    }
    
    // Setup sensitive detectors for active volumes
    SetupSensitiveDetectors();
    
    return worldPV;
}

/**
 * @brief Create a G4VSolid from JSON configuration
 * @param config JSON configuration for the solid
 * @param name Name for the solid
 * @return Pointer to created G4VSolid
 * @throws std::runtime_error if solid creation fails
 * @details Creates a solid based on the type specified in the config.
 *          Supports all basic Geant4 shapes and boolean operations.
 */
G4VSolid* GeometryParser::CreateSolid(const json& config, const std::string& name) {
    // Check if solid already exists in cache
    G4cout << "Checking solid cache for " << name << G4endl;
    if (solids.find(name) != solids.end()) {
        G4cout << "Solid " << name << " already exists in cache" << G4endl;
        return solids[name];
    }
    
    G4cout << "Creating solid " << name << G4endl;
    
    // Debug output to check config structure
    G4cout << "Config keys available:";
    for (auto it = config.begin(); it != config.end(); ++it) {
        G4cout << " " << it.key();
    }
    G4cout << G4endl;
    
    // Ensure type exists
    if (!config.contains("type")) {
        G4cerr << "Error: No 'type' field in solid config for " << name << G4endl;
        throw std::runtime_error("Missing 'type' field in solid config");
    }
    
    G4VSolid* solid = nullptr;
    std::string type = config["type"].get<std::string>();
    G4cout << "Solid type: " << type << G4endl;
    
    // Check for dimensions object - only required for basic shapes, not for boolean operations
    bool needsDimensions = (type != "union" && type != "subtraction" && type != "intersection");
    
    if (needsDimensions && !config.contains("dimensions")) {
        G4cerr << "Error: dimensions not found in solid " << name << " of type " << type << G4endl;
        exit(1);
    }
    
    // Get dimensions object for easier access (only for basic shapes)
    const json* dimsPtr = nullptr;
    if (config.contains("dimensions")) {
        dimsPtr = &config["dimensions"];
    }
    const json& dims = dimsPtr ? *dimsPtr : json::object();
    
    // Dispatch to appropriate shape creation function based on type
    if (type == "union") {
        // Check if this is a new-style union with components
        //if (config.contains("components") && !config["components"].empty()) {
        G4cout << "Creating union solid from components: " << name << G4endl;
        solid = CreateBooleanSolidFromComponents(config, name);
        //} else {
            // Legacy boolean operation with solid1 and solid2
            //solid = CreateBooleanSolid(config, name);
        //}
    }
    //else if (type == "subtraction" || type == "intersection") {
    //    solid = CreateBooleanSolid(config, name);
    //}
    else if (type == "box") {
        solid = CreateBoxSolid(dims, name);
    }
    else if (type == "sphere") {
        solid = CreateSphereSolid(dims, name);
    }
    else if (type == "cylinder" || type == "tube") {
        solid = CreateCylinderSolid(dims, name);
    }
    else if (type == "cone") {
        solid = CreateConeSolid(dims, name);
    }
    else if (type == "trd" || type == "trapezoid") {
        solid = CreateTrapezoidSolid(dims, name);
    }
    else if (type == "torus") {
        solid = CreateTorusSolid(dims, name);
    }
    else if (type == "ellipsoid") {
        solid = CreateEllipsoidSolid(dims, name);
    }
    else if (type == "orb") {
        solid = CreateOrbSolid(dims, name);
    }
    else if (type == "elliptical_tube") {
        solid = CreateEllipticalTubeSolid(dims, name);
    }
    else if (type == "polycone") {
        solid = CreatePolyconeSolid(config, dims, name);
    }
    else if (type == "polyhedra") {
        solid = CreatePolyhedraSolid(config, dims, name);
    }
    else {
        throw std::runtime_error("Unsupported solid type: " + type);
    }
    
    // Cache the solid
    solids[name] = solid;
    return solid;
}

/**
 * @brief Create a boolean solid from components in the new format
 * @param config JSON configuration for the union with components
 * @param name Name for the resulting solid
 * @return Pointer to created G4VSolid
 * @throws std::runtime_error if boolean operation fails
 * @details Creates a boolean solid by recursively combining multiple components.
 *          First processes all union operations, then all subtraction operations.
 */
G4VSolid* GeometryParser::CreateBooleanSolidFromComponents(const json& config, const std::string& name) {
    // Debug the entire config structure
    G4cout << "Boolean solid config keys:";
    for (auto it = config.begin(); it != config.end(); ++it) {
        G4cout << " " << it.key();
    }
    G4cout << G4endl;
    
    // Ensure we have components
    if (!config.contains("components") || config["components"].empty()) {
        throw std::runtime_error("No components found for boolean solid: " + name);
    }
    
    const auto& components = config["components"];
    G4cout << "Creating boolean solid " << name << " with " << components.size() << " components" << G4endl;
    
    // Debug each component structure
    for (size_t i = 0; i < components.size(); ++i) {
        const auto& comp = components[i];
        G4cout << "Component " << i << " keys:";
        for (auto it = comp.begin(); it != comp.end(); ++it) {
            G4cout << " " << it.key();
        }
        G4cout << G4endl;
    }
    
    // First, separate union and subtraction components
    std::vector<json> unionComponents;
    std::vector<json> subtractionComponents;
    
    for (const auto& component : components) {
        // Debug output to see component structure
        G4cout << "Processing component: " << component["name"].get<std::string>() << G4endl;
        
        std::string operation = "union"; // Default operation is union
        if (component.contains("boolean_operation")) {
            operation = component["boolean_operation"].get<std::string>();
            G4cout << "  Boolean operation: " << operation << G4endl;
        }
        
        if (operation == "union" || operation == "add") {
            unionComponents.push_back(component);
        } else if (operation == "subtract") {
            subtractionComponents.push_back(component);
        } else {
            G4cout << "Warning: Unknown boolean operation: " << operation << ", treating as union" << G4endl;
            unionComponents.push_back(component);
        }
    }
    
    G4cout << "Found " << unionComponents.size() << " union components and " 
           << subtractionComponents.size() << " subtraction components" << G4endl;
    
    // We need at least one union component to start with
    if (unionComponents.empty()) {
        throw std::runtime_error("No union components found for boolean solid: " + name);
    }
    
    // Create the first component solid
    const auto& firstComponent = unionComponents[0];
    
    // Check if the first component has all required keys
    if (!firstComponent.contains("name")) {
        G4cerr << "Error: First component missing 'name' key" << G4endl;
        throw std::runtime_error("First component missing 'name' key");
    }
    std::string firstCompName = firstComponent["name"].get<std::string>();
    
    // Debug the first component
    G4cout << "First component details:" << G4endl;
    G4cout << "  Name: " << firstCompName << G4endl;
    
    // Check for type key
    if (!firstComponent.contains("type")) {
        G4cerr << "Error: First component missing 'type' key" << G4endl;
        throw std::runtime_error("First component missing 'type' key");
    }
    G4cout << "  Type: " << firstComponent["type"].get<std::string>() << G4endl;
    
    // Check for dimensions
    G4cout << "  Has dimensions: " << (firstComponent.contains("dimensions") ? "yes" : "no") << G4endl;
    if (firstComponent.contains("dimensions")) {
        G4cout << "  Dimensions keys: ";
        for (auto it = firstComponent["dimensions"].begin(); it != firstComponent["dimensions"].end(); ++it) {
            G4cout << " " << it.key();
        }
        G4cout << G4endl;
    }
    
    // Create the first solid
    G4cout << "Creating first component solid: " << firstCompName << G4endl;
    G4VSolid* resultSolid = CreateSolid(firstComponent, firstCompName);
    G4cout << "Created first component solid: " << firstCompName << G4endl;
    
    // Process remaining union components
    for (size_t i = 1; i < unionComponents.size(); i++) {
        const auto& component = unionComponents[i];
        
        // Check if component has required keys
        if (!component.contains("name")) {
            G4cerr << "Error: Union component " << i << " missing 'name' key" << G4endl;
            continue; // Skip this component
        }
        std::string compName = component["name"].get<std::string>();
        
        // Create the component solid
        G4cout << "Creating union component solid: " << compName << G4endl;
        G4VSolid* componentSolid = nullptr;
        
        try {
            componentSolid = CreateSolid(component, compName);
        } catch (const std::exception& e) {
            G4cerr << "Error creating component solid: " << e.what() << G4endl;
            continue; // Skip this component
        }
        
        // Get the component's position and rotation
        G4ThreeVector position(0, 0, 0);
        G4RotationMatrix* rotation = nullptr;
        
        // Get position and rotation from the first placement
        if (component.contains("placements") && !component["placements"].empty()) {
            const auto& placement = component["placements"][0];
            
            // Check for required position keys
            bool validPosition = true;
            if (!placement.contains("x")) {
                G4cerr << "Warning: Missing 'x' in placement for " << compName << G4endl;
                validPosition = false;
            }
            if (!placement.contains("y")) {
                G4cerr << "Warning: Missing 'y' in placement for " << compName << G4endl;
                validPosition = false;
            }
            if (!placement.contains("z")) {
                G4cerr << "Warning: Missing 'z' in placement for " << compName << G4endl;
                validPosition = false;
            }
            
            if (validPosition) {
                position = G4ThreeVector(
                    placement["x"].get<double>() * mm,
                    placement["y"].get<double>() * mm,
                    placement["z"].get<double>() * mm
                );
            }
            
            if (placement.contains("rotation")) {
                try {
                    rotation = ParseRotation(placement["rotation"]);
                } catch (const std::exception& e) {
                    G4cerr << "Error parsing rotation: " << e.what() << G4endl;
                    // Continue with null rotation
                }
            }
        }
        
        // Create a union solid
        std::string unionName = name + "_union_" + std::to_string(i);
        G4cout << "Creating union solid: " << unionName << G4endl;
        G4UnionSolid* unionSolid = new G4UnionSolid(
            unionName, resultSolid, componentSolid, rotation, position);
        
        G4cout << "Added union component " << compName << " at position " 
               << position << G4endl;
        
        // Update the result solid
        resultSolid = unionSolid;
    }
    
    // Process subtraction components
    for (size_t i = 0; i < subtractionComponents.size(); i++) {
        const auto& component = subtractionComponents[i];
        
        // Check if component has required keys
        if (!component.contains("name")) {
            G4cerr << "Error: Subtraction component " << i << " missing 'name' key" << G4endl;
            continue; // Skip this component
        }
        std::string compName = component["name"].get<std::string>();
        
        // Create the component solid
        G4cout << "Creating subtraction component solid: " << compName << G4endl;
        G4VSolid* componentSolid = nullptr;
        
        try {
            componentSolid = CreateSolid(component, compName);
        } catch (const std::exception& e) {
            G4cerr << "Error creating subtraction component solid: " << e.what() << G4endl;
            continue; // Skip this component
        }
        
        // Get the component's position and rotation
        G4ThreeVector position(0, 0, 0);
        G4RotationMatrix* rotation = nullptr;
        
        // Get position and rotation from the first placement
        if (component.contains("placements") && !component["placements"].empty()) {
            const auto& placement = component["placements"][0];
            
            // Check for required position keys
            bool validPosition = true;
            if (!placement.contains("x")) {
                G4cerr << "Warning: Missing 'x' in placement for " << compName << G4endl;
                validPosition = false;
            }
            if (!placement.contains("y")) {
                G4cerr << "Warning: Missing 'y' in placement for " << compName << G4endl;
                validPosition = false;
            }
            if (!placement.contains("z")) {
                G4cerr << "Warning: Missing 'z' in placement for " << compName << G4endl;
                validPosition = false;
            }
            
            if (validPosition) {
                position = G4ThreeVector(
                    placement["x"].get<double>() * mm,
                    placement["y"].get<double>() * mm,
                    placement["z"].get<double>() * mm
                );
            }
            
            if (placement.contains("rotation")) {
                try {
                    rotation = ParseRotation(placement["rotation"]);
                } catch (const std::exception& e) {
                    G4cerr << "Error parsing rotation: " << e.what() << G4endl;
                    // Continue with null rotation
                }
            }
        }
        
        // Create a subtraction solid
        std::string subtractName = name + "_subtract_" + std::to_string(i);
        G4SubtractionSolid* subtractSolid = new G4SubtractionSolid(
            subtractName, resultSolid, componentSolid, rotation, position);
        
        G4cout << "Subtracted component " << compName << " at position " 
               << position << G4endl;
        
        // Update the result solid
        resultSolid = subtractSolid;
    }
    
    // Cache and return the final solid
    solids[name] = resultSolid;
    G4cout << "Created boolean solid: " << name << G4endl;
    return resultSolid;
}

/**
 * @brief Create a boolean solid (union, subtraction, intersection)
 * @param config JSON configuration for the boolean operation
 * @param name Name for the resulting solid
 * @return Pointer to created G4VSolid
 * @throws std::runtime_error if boolean operation fails
 * @details Creates a boolean solid by combining two or more solids.
 *          For unions, supports multiple components in the new format.
 */
G4VSolid* GeometryParser::CreateBooleanSolid(const json& config, const std::string& name) {
    std::string type = config["type"].get<std::string>();
    
    // Get the first and second solids
    G4VSolid* solid1 = nullptr;
    G4VSolid* solid2 = nullptr;
    
    // First solid can be a reference or inline definition
    if (config["solid1"].is_string()) {
        std::string solid1Name = config["solid1"].get<std::string>();
        if (solids.find(solid1Name) == solids.end()) {
            throw std::runtime_error("Referenced solid not found: " + solid1Name);
        }
        solid1 = solids[solid1Name];
    } else {
        solid1 = CreateSolid(config["solid1"], name + "_solid1");
    }
    
    // Second solid can be a reference or inline definition
    if (config["solid2"].is_string()) {
        std::string solid2Name = config["solid2"].get<std::string>();
        if (solids.find(solid2Name) == solids.end()) {
            throw std::runtime_error("Referenced solid not found: " + solid2Name);
        }
        solid2 = solids[solid2Name];
    } else {
        solid2 = CreateSolid(config["solid2"], name + "_solid2");
    }
    
    // Get transformation for second solid
    G4ThreeVector position(0, 0, 0);
    G4RotationMatrix* rotation = nullptr;
    
    // Check for placement object first
    if (config.contains("placement")) {
        ParsePlacement(config, position, rotation);
    }
    // Check for relative_position, then fall back to position for backward compatibility
    else if (config.contains("relative_position")) {
        position = ParseVector(config["relative_position"]);
        
        if (config.contains("relative_rotation")) {
            rotation = ParseRotation(config["relative_rotation"]);
        }
    } 
    // Legacy format
    else if (config.contains("position") && !config.contains("mother_volume")) {
        // Only use position if this is not a volume placement (no mother_volume)
        position = ParseVector(config["position"]);
        
        if (config.contains("rotation") && !config.contains("mother_volume")) {
            rotation = ParseRotation(config["rotation"]);
        }
    }
    
    G4BooleanSolid* booleanSolid = nullptr;
    // Create the boolean solid
    if (type == "union") {
        booleanSolid = new G4UnionSolid(name, solid1, solid2, rotation, position);
    }
    else if (type == "subtraction") {
        booleanSolid = new G4SubtractionSolid(name, solid1, solid2, rotation, position);
    }
    else if (type == "intersection") {
        booleanSolid = new G4IntersectionSolid(name, solid1, solid2, rotation, position);
    }
    else {
        throw std::runtime_error("Invalid boolean operation: " + type);
    }
    
    
    if (!booleanSolid) {
        throw std::runtime_error("Failed to create boolean solid: " + name);
    }
    
    // Cache the boolean solid
    solids[name] = booleanSolid;
    return booleanSolid;
}

/**
 * @brief Load and parse an external JSON geometry file
 * @param filename Path to the external JSON file
 * @return JSON object containing the parsed file
 * @throws std::runtime_error if file cannot be opened or parsed
 * @details Loads an external JSON file, resolving the path relative to the
 *          main configuration file's directory.
 */
json GeometryParser::LoadExternalGeometry(const std::string& filename) {
    // Resolve path relative to the main config file
    std::string fullPath = configPath + "/" + filename;
    
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open external geometry file: " + fullPath);
    }
    
    json externalConfig;
    file >> externalConfig;
    return externalConfig;
}

/**
 * @brief Import an assembled geometry from an external JSON file
 * @param config JSON configuration for the import
 * @param parentVolume Parent logical volume to place the imported geometry in
 * @throws std::runtime_error if import fails
 * @details Imports a geometry defined in an external JSON file and places it
 *          in the parent volume with the specified transformation.
 */
/**
 * @brief Setup sensitive detectors for active volumes
 * @details This method assigns sensitive detectors to volumes marked as active in the JSON config
 */
void GeometryParser::SetupSensitiveDetectors() {
    // Get the SD manager
    G4SDManager* sdManager = G4SDManager::GetSDMpointer();
    
    // Create and register the default sensitive detector
    MySensitiveDetector* mySD = new MySensitiveDetector("MySD");
    sdManager->AddNewDetector(mySD);
    
    // Iterate through all volumes in the JSON configuration
    for (const auto& volConfig : geometryConfig["volumes"]) {
        // Check if this volume is marked as active
        if (volConfig.contains("isActive") && volConfig["isActive"].get<bool>()) {
            // Get the hits collection name (default to "MyHitsCollection" if not specified)
            std::string hitsCollName = "MyHitsCollection";
            if (volConfig.contains("hitsCollectionName")) {
                hitsCollName = volConfig["hitsCollectionName"].get<std::string>();
            }
            
            // Only assign to MyHitsCollection for now
            if (hitsCollName == "MyHitsCollection") {
                // Get the logical volume
                std::string volName = volConfig["name"].get<std::string>();
                G4LogicalVolume* logicalVol = logicalVolumeMap[volName + "_logical"];
                
                if (logicalVol) {
                    // Assign the sensitive detector to this logical volume
                    G4cout << "Setting " << volName << " as sensitive detector" << G4endl;
                    logicalVol->SetSensitiveDetector(mySD);
                }
            }
        }
    }
}

/**
 * @brief Import an assembled geometry from an external JSON file
 * @param config JSON configuration for the import
 * @param parentVolume Parent logical volume to place the imported geometry in
 * @throws std::runtime_error if import fails
 * @details Imports a geometry defined in an external JSON file and places it
 *          in the parent volume with the specified transformation.
 */
void GeometryParser::CreateAssembly(const json& config) {
    // Get the assembly name
    std::string assemblyName = config["name"].get<std::string>();
    G4cout << "Creating assembly " << assemblyName << G4endl;

    // Create a new assembly volume
    G4AssemblyVolume* assembly = new G4AssemblyVolume();

    // Store the assembly in the assemblies map
    assemblies[assemblyName] = assembly;

    // Loop through all components of this assembly
    G4cout << "Looping through components of assembly " << assemblyName << G4endl;
    for (const auto& compConfig : config["components"]) {
        std::string childName = compConfig["name"].get<std::string>();
        G4cout << "Processing component " << childName << " of assembly " << assemblyName << G4endl;

        // Skip if this is another assembly (to avoid circular references)
        if (compConfig["type"].get<std::string>() == "assembly") {
            G4cout << "Warning: Assembly " << childName
                   << " is a component of assembly " << assemblyName
                   << ". Nested assemblies are not supported." << G4endl;
            continue;
        }

        // Create the logical volume for this component if it doesn't exist yet
        G4LogicalVolume* childLV = nullptr;
        if (volumes.find(childName) == volumes.end()) {
            childLV = CreateVolume(compConfig);
        } else {
            childLV = volumes[childName];
        }

        // Each component can have multiple placements
        if (!compConfig.contains("placements") || compConfig["placements"].empty()) {
            G4cout << "Warning: No placements for component " << childName << " in assembly " << assemblyName << G4endl;
            continue;
        }
        for (const auto& placement : compConfig["placements"]) {
            G4ThreeVector position;
            G4RotationMatrix* rotation = nullptr;
            // Use the placement info (relative to assembly)
            ParsePlacement(placement, position, rotation);
            G4cout << "Adding " << childName << " to assembly " << assemblyName << " at position " << position << G4endl;
            assembly->AddPlacedVolume(childLV, position, rotation);
        }
    }


    G4cout << "Created assembly " << assemblyName << G4endl;
}


/**
 * @brief Import an assembled geometry from an external JSON file
 * @param config JSON configuration for the import
 * @param parentVolume Parent logical volume to place the imported geometry in
 * @details Imports a geometry defined in an external JSON file and places it
 *          in the parent volume with the specified transformation.
 */
void GeometryParser::ImportAssembledGeometry(const json& config, G4LogicalVolume* parentVolume) {
    // Load the external file
    std::string filename = config["external_file"].get<std::string>();
    json externalConfig = LoadExternalGeometry(filename);
    
    // Get transformation for the assembly using the new ParsePlacement function
    // which handles both the new placement format and legacy format
    G4ThreeVector position;
    G4RotationMatrix* rotation = nullptr;
    ParsePlacement(config, position, rotation);
    
    // Create volumes from the external file
    std::map<std::string, G4LogicalVolume*> externalVolumes;
    
    // First, create all logical volumes
    for (const auto& volConfig : externalConfig["volumes"]) {
        std::string name = volConfig["name"].get<std::string>();
        
        // If a name prefix is specified, add it to avoid name collisions
        if (config.contains("name_prefix")) {
            name = config["name_prefix"].get<std::string>() + "_" + name;
        }
        
        // Create the volume
        G4LogicalVolume* logicalVolume = CreateVolume(volConfig);
        volumes[volConfig["name"].get<std::string>()] = logicalVolume;
        
        // Store in logical volume map with _logical suffix for sensitive detector setup
        logicalVolumeMap[volConfig["name"].get<std::string>() + "_logical"] = logicalVolume;
    }
    
    // Then, place all volumes
    for (const auto& volConfig : externalConfig["volumes"]) {
        std::string name = volConfig["name"].get<std::string>();
        
        // If a name prefix is specified, add it to avoid name collisions
        if (config.contains("name_prefix")) {
            name = config["name_prefix"].get<std::string>() + "_" + name;
        }
        
        // Skip the root volume, which will be placed in the parent volume
        if (volConfig.contains("root") && volConfig["root"].get<bool>()) {
            G4LogicalVolume* rootVolume = externalVolumes[name];
            
            // Place the root volume in the parent volume
            new G4PVPlacement(rotation, position, rootVolume, name, parentVolume, false, 0);
            continue;
        }
        
        // Place the volume in its mother volume
        if (volConfig.contains("mother_volume")) {
            std::string motherName = volConfig["mother_volume"].get<std::string>();
            
            // If a name prefix is specified, add it to the mother name too
            if (config.contains("name_prefix")) {
                motherName = config["name_prefix"].get<std::string>() + "_" + motherName;
            }
            
            G4LogicalVolume* motherVolume = externalVolumes[motherName];
            G4LogicalVolume* logicalVolume = externalVolumes[name];
            
            // Parse position and rotation using the new ParsePlacement function
            G4ThreeVector volPosition;
            G4RotationMatrix* volRotation = nullptr;
            ParsePlacement(volConfig, volPosition, volRotation);
            
            new G4PVPlacement(volRotation, volPosition, logicalVolume, name, motherVolume, false, 0);
        }
    }
}
// Box solid creation function
G4VSolid* GeometryParser::CreateBoxSolid(const json& dims, const std::string& name) {
    // All dimensions are in mm, but we need to divide by 2 for half-dimensions
    G4double dx = dims["x"].get<double>() * mm / 2;
    G4double dy = dims["y"].get<double>() * mm / 2;
    G4double dz = dims["z"].get<double>() * mm / 2;
    return new G4Box(name, dx, dy, dz);
}

// Sphere solid creation function
G4VSolid* GeometryParser::CreateSphereSolid(const json& dims, const std::string& name) {
    // Get radius (required)
    G4double rmax = dims["radius"].get<double>() * mm;
    
    // Get optional parameters with defaults
    G4double rmin = dims.contains("inner_radius") ? dims["inner_radius"].get<double>() * mm : 0;
    G4double sphi = dims.contains("start_phi") ? dims["start_phi"].get<double>() * rad : 0;
    G4double dphi = dims.contains("delta_phi") ? dims["delta_phi"].get<double>() * rad : 2 * M_PI * rad;
    G4double stheta = dims.contains("start_theta") ? dims["start_theta"].get<double>() * rad : 0;
    G4double dtheta = dims.contains("delta_theta") ? dims["delta_theta"].get<double>() * rad : M_PI * rad;
    
    return new G4Sphere(name, rmin, rmax, sphi, dphi, stheta, dtheta);
}

// Cylinder/tube solid creation function
G4VSolid* GeometryParser::CreateCylinderSolid(const json& dims, const std::string& name) {
    // Get required parameters
    G4double rmax = dims["radius"].get<double>() * mm;
    G4double hz = dims["height"].get<double>() * mm / 2; // Half-height for G4Tubs
    
    // Get optional parameters with defaults
    G4double rmin = dims.contains("inner_radius") ? dims["inner_radius"].get<double>() * mm : 0;
    G4double sphi = dims.contains("start_phi") ? dims["start_phi"].get<double>() * rad : 0;
    G4double dphi = dims.contains("delta_phi") ? dims["delta_phi"].get<double>() * rad : 2 * M_PI * rad;
    
    return new G4Tubs(name, rmin, rmax, hz, sphi, dphi);
}

// Cone solid creation function
G4VSolid* GeometryParser::CreateConeSolid(const json& dims, const std::string& name) {
    // Get required parameters
    G4double rmax1 = dims.contains("radius1") ? dims["radius1"].get<double>() * mm : 
                    dims["rmax1"].get<double>() * mm;
    G4double rmax2 = dims.contains("radius2") ? dims["radius2"].get<double>() * mm : 
                    dims["rmax2"].get<double>() * mm;
    G4double hz = dims.contains("height") ? dims["height"].get<double>() * mm / 2 : 
                 dims["hz"].get<double>() * mm;
    
    // Get optional parameters with defaults
    G4double rmin1 = 0;
    if (dims.contains("inner_radius1")) {
        rmin1 = dims["inner_radius1"].get<double>() * mm;
    } else if (dims.contains("rmin1")) {
        rmin1 = dims["rmin1"].get<double>() * mm;
    }
    
    G4double rmin2 = 0;
    if (dims.contains("inner_radius2")) {
        rmin2 = dims["inner_radius2"].get<double>() * mm;
    } else if (dims.contains("rmin2")) {
        rmin2 = dims["rmin2"].get<double>() * mm;
    }
    
    G4double sphi = dims.contains("start_phi") ? dims["start_phi"].get<double>() * rad : 0;
    G4double dphi = dims.contains("delta_phi") ? dims["delta_phi"].get<double>() * rad : 2 * M_PI * rad;
    
    return new G4Cons(name, rmin1, rmax1, rmin2, rmax2, hz, sphi, dphi);
}

// Trapezoid solid creation function
G4VSolid* GeometryParser::CreateTrapezoidSolid(const json& dims, const std::string& name) {
    // All values are in mm, but we need to divide by 2 for half-dimensions
    G4double x1 = dims.contains("dx1") ? dims["dx1"].get<double>() * mm / 2 : 
                 dims["x1"].get<double>() * mm / 2;
    
    G4double x2 = dims.contains("dx2") ? dims["dx2"].get<double>() * mm / 2 : 
                 dims["x2"].get<double>() * mm / 2;
    
    G4double y1 = dims.contains("dy1") ? dims["dy1"].get<double>() * mm / 2 : 
                 dims["y1"].get<double>() * mm / 2;
    
    G4double y2 = dims.contains("dy2") ? dims["dy2"].get<double>() * mm / 2 : 
                 dims["y2"].get<double>() * mm / 2;
    
    G4double hz = dims.contains("dz") ? dims["dz"].get<double>() * mm : 
                 dims["height"].get<double>() * mm / 2;
    
    return new G4Trd(name, x1, x2, y1, y2, hz);
}

// Torus solid creation function
G4VSolid* GeometryParser::CreateTorusSolid(const json& dims, const std::string& name) {
    // Get required parameters
    G4double rmax = dims.contains("tube_radius") ? dims["tube_radius"].get<double>() * mm :
                  dims["minor_radius"].get<double>() * mm;
    
    G4double rtor = dims.contains("torus_radius") ? dims["torus_radius"].get<double>() * mm :
                  dims["major_radius"].get<double>() * mm;
    
    // Get optional parameters with defaults
    G4double rmin = dims.contains("inner_radius") ? dims["inner_radius"].get<double>() * mm : 0;
    G4double sphi = dims.contains("start_phi") ? dims["start_phi"].get<double>() * rad : 0;
    G4double dphi = dims.contains("delta_phi") ? dims["delta_phi"].get<double>() * rad : 2 * M_PI * rad;
    
    return new G4Torus(name, rmin, rmax, rtor, sphi, dphi);
}

// Ellipsoid solid creation function
G4VSolid* GeometryParser::CreateEllipsoidSolid(const json& dims, const std::string& name) {
    // Get required parameters (semi-axes)
    G4double ax = dims.contains("ax") ? dims["ax"].get<double>() * mm :
                dims["x_radius"].get<double>() * mm;
    
    G4double by = dims.contains("by") ? dims["by"].get<double>() * mm :
                dims["y_radius"].get<double>() * mm;
    
    G4double cz = dims.contains("cz") ? dims["cz"].get<double>() * mm :
                dims["z_radius"].get<double>() * mm;
    
    // Set default z cuts and override if specified
    G4double zcut1 = dims.contains("zcut1") ? dims["zcut1"].get<double>() * mm : -cz;
    G4double zcut2 = dims.contains("zcut2") ? dims["zcut2"].get<double>() * mm : cz;
    
    return new G4Ellipsoid(name, ax, by, cz, zcut1, zcut2);
}

// Orb solid creation function
G4VSolid* GeometryParser::CreateOrbSolid(const json& dims, const std::string& name) {
    // Get radius (required)
    G4double radius = dims["radius"].get<double>() * mm;
    
    return new G4Orb(name, radius);
}

// Elliptical tube solid creation function
G4VSolid* GeometryParser::CreateEllipticalTubeSolid(const json& dims, const std::string& name) {
    // Get required parameters
    G4double dx = dims.contains("dx") ? dims["dx"].get<double>() * mm :
                dims["x"].get<double>() * mm;
    
    G4double dy = dims.contains("dy") ? dims["dy"].get<double>() * mm :
                dims["y"].get<double>() * mm;
    
    // For z dimension, check multiple possible property names
    G4double dz;
    if (dims.contains("dz")) {
        dz = dims["dz"].get<double>() * mm;
    } else if (dims.contains("z")) {
        dz = dims["z"].get<double>() * mm;
    } else {
        dz = dims["height"].get<double>() * mm / 2;
    }
    
    return new G4EllipticalTube(name, dx, dy, dz);
}

// Polycone solid creation function
G4VSolid* GeometryParser::CreatePolyconeSolid(const json& config, const json& dims, const std::string& name) {
    // Get optional angular parameters with defaults
    G4double sphi = dims.contains("start_phi") ? dims["start_phi"].get<double>() * rad : 0;
    G4double dphi = dims.contains("delta_phi") ? dims["delta_phi"].get<double>() * rad : 2 * M_PI * rad;
    
    // Get z planes and radii (all values in mm)
    std::vector<G4double> z_planes;
    std::vector<G4double> rmin;
    std::vector<G4double> rmax;
    
    // Check if z, rmin, rmax arrays are in the dimensions object
    if (dims.contains("z") && dims.contains("rmax")) {
        // Get arrays from dimensions object
        const auto& z_array = dims["z"];
        const auto& rmax_array = dims["rmax"];
        
        // Check if there's an rmin array, otherwise use zeros
        bool has_rmin = dims.contains("rmin");
        const auto& rmin_array = has_rmin ? dims["rmin"] : json::array();
        
        // Fill the vectors
        for (size_t i = 0; i < z_array.size(); i++) {
            z_planes.push_back(z_array[i].get<double>() * mm);
            rmax.push_back(rmax_array[i].get<double>() * mm);
            rmin.push_back(has_rmin ? rmin_array[i].get<double>() * mm : 0);
        }
    }
    // Check if planes array is in the dimensions
    else if (dims.contains("planes")) {
        for (const auto& plane : dims["planes"]) {
            z_planes.push_back(plane["z"].get<double>() * mm);
            rmin.push_back(plane.contains("rmin") ? plane["rmin"].get<double>() * mm : 0);
            rmax.push_back(plane["rmax"].get<double>() * mm);
        }
    }
    // Legacy format - check if planes array is directly in the config
    else if (config.contains("planes")) {
        for (const auto& plane : config["planes"]) {
            z_planes.push_back(plane["z"].get<double>() * mm);
            rmin.push_back(plane.contains("rmin") ? plane["rmin"].get<double>() * mm : 0);
            rmax.push_back(plane["rmax"].get<double>() * mm);
        }
    }
    
    // Sort z-planes and corresponding radii if not already sorted
    if (z_planes.size() >= 2) {
        // Create a vector of indices
        std::vector<size_t> indices(z_planes.size());
        for (size_t i = 0; i < indices.size(); ++i) {
            indices[i] = i;
        }
        
        // Sort indices based on z values
        std::sort(indices.begin(), indices.end(),
                 [&z_planes](size_t a, size_t b) { return z_planes[a] < z_planes[b]; });
        
        // Check if already sorted
        bool isSorted = true;
        for (size_t i = 0; i < indices.size(); ++i) {
            if (indices[i] != i) {
                isSorted = false;
                break;
            }
        }
        
        // If not sorted, reorder the vectors
        if (!isSorted) {
            G4cout << "Sorting z-planes for polycone " << name << G4endl;
            
            // Create temporary vectors with sorted values
            std::vector<G4double> sorted_z(z_planes.size());
            std::vector<G4double> sorted_rmin(rmin.size());
            std::vector<G4double> sorted_rmax(rmax.size());
            
            for (size_t i = 0; i < indices.size(); ++i) {
                sorted_z[i] = z_planes[indices[i]];
                sorted_rmin[i] = rmin[indices[i]];
                sorted_rmax[i] = rmax[indices[i]];
            }
            
            // Replace original vectors with sorted ones
            z_planes = sorted_z;
            rmin = sorted_rmin;
            rmax = sorted_rmax;
        }
    }
    
    // Validate that z-planes are in ascending order and rmin < rmax
    if (z_planes.size() < 2) {
        G4cerr << "Error in polycone " << name << ": need at least 2 z-planes, found " 
               << z_planes.size() << G4endl;
        exit(1);
    }
    
    for (size_t i = 1; i < z_planes.size(); i++) {
        if (z_planes[i] <= z_planes[i-1]) {
            G4cerr << "Error in polycone " << name << ": z-planes must be in ascending order. "
                   << "Found z[" << i-1 << "] = " << z_planes[i-1]/mm << " mm >= z[" 
                   << i << "] = " << z_planes[i]/mm << " mm" << G4endl;
            exit(1);
        }
    }
    
    for (size_t i = 0; i < z_planes.size(); i++) {
        if (rmin[i] >= rmax[i]) {
            G4cerr << "Error in polycone " << name << ": rmin must be less than rmax. "
                   << "Found at z[" << i << "] = " << z_planes[i]/mm << " mm: rmin = " 
                   << rmin[i]/mm << " mm >= rmax = " << rmax[i]/mm << " mm" << G4endl;
            exit(1);
        }
    }
    
    // Create the polycone solid
    try {
        return new G4Polycone(name, sphi, dphi, z_planes.size(), 
                            z_planes.data(), rmin.data(), rmax.data());
    } catch (const std::exception& e) {
        G4cerr << "Error creating polycone " << name << ": " << e.what() << G4endl;
        G4cerr << "Z planes: ";
        for (size_t i = 0; i < z_planes.size(); i++) {
            G4cerr << z_planes[i]/mm << " ";
        }
        G4cerr << "\nRmin: ";
        for (size_t i = 0; i < rmin.size(); i++) {
            G4cerr << rmin[i]/mm << " ";
        }
        G4cerr << "\nRmax: ";
        for (size_t i = 0; i < rmax.size(); i++) {
            G4cerr << rmax[i]/mm << " ";
        }
        G4cerr << G4endl;
        exit(1);
    }
}

// Polyhedra solid creation function
G4VSolid* GeometryParser::CreatePolyhedraSolid(const json& config, const json& dims, const std::string& name) {
    // Get optional angular parameters with defaults
    G4double sphi = dims.contains("start_phi") ? dims["start_phi"].get<double>() * rad : 0;
    G4double dphi = dims.contains("delta_phi") ? dims["delta_phi"].get<double>() * rad : 2 * M_PI * rad;
    G4int numSides = dims.contains("num_sides") ? dims["num_sides"].get<int>() : 8;
    
    // Get z planes and radii (all values in mm)
    std::vector<G4double> z_planes;
    std::vector<G4double> rmin;
    std::vector<G4double> rmax;
    
    // Check if z, rmin, rmax arrays are in the dimensions object
    if (dims.contains("z") && dims.contains("rmax")) {
        // Get arrays from dimensions object
        const auto& z_array = dims["z"];
        const auto& rmax_array = dims["rmax"];
        
        // Check if there's an rmin array, otherwise use zeros
        bool has_rmin = dims.contains("rmin");
        const auto& rmin_array = has_rmin ? dims["rmin"] : json::array();
        
        // Fill the vectors
        for (size_t i = 0; i < z_array.size(); i++) {
            z_planes.push_back(z_array[i].get<double>() * mm);
            rmax.push_back(rmax_array[i].get<double>() * mm);
            rmin.push_back(has_rmin ? rmin_array[i].get<double>() * mm : 0);
        }
    }
    // Check if planes array is in the dimensions
    else if (dims.contains("planes")) {
        for (const auto& plane : dims["planes"]) {
            z_planes.push_back(plane["z"].get<double>() * mm);
            rmin.push_back(plane.contains("rmin") ? plane["rmin"].get<double>() * mm : 0);
            rmax.push_back(plane["rmax"].get<double>() * mm);
        }
    }
    // Legacy format - check if planes array is directly in the config
    else if (config.contains("planes")) {
        for (const auto& plane : config["planes"]) {
            z_planes.push_back(plane["z"].get<double>() * mm);
            rmin.push_back(plane.contains("rmin") ? plane["rmin"].get<double>() * mm : 0);
            rmax.push_back(plane["rmax"].get<double>() * mm);
        }
    }
    
    // Sort z-planes and corresponding radii if not already sorted
    if (z_planes.size() >= 2) {
        // Create a vector of indices
        std::vector<size_t> indices(z_planes.size());
        for (size_t i = 0; i < indices.size(); ++i) {
            indices[i] = i;
        }
        
        // Sort indices based on z values
        std::sort(indices.begin(), indices.end(),
                 [&z_planes](size_t a, size_t b) { return z_planes[a] < z_planes[b]; });
        
        // Check if already sorted
        bool isSorted = true;
        for (size_t i = 0; i < indices.size(); ++i) {
            if (indices[i] != i) {
                isSorted = false;
                break;
            }
        }
        
        // If not sorted, reorder the vectors
        if (!isSorted) {
            G4cout << "Sorting z-planes for polyhedra " << name << G4endl;
            
            // Create temporary vectors with sorted values
            std::vector<G4double> sorted_z(z_planes.size());
            std::vector<G4double> sorted_rmin(rmin.size());
            std::vector<G4double> sorted_rmax(rmax.size());
            
            for (size_t i = 0; i < indices.size(); ++i) {
                sorted_z[i] = z_planes[indices[i]];
                sorted_rmin[i] = rmin[indices[i]];
                sorted_rmax[i] = rmax[indices[i]];
            }
            
            // Replace original vectors with sorted ones
            z_planes = sorted_z;
            rmin = sorted_rmin;
            rmax = sorted_rmax;
        }
    }
    
    // Validate that z-planes are in ascending order and rmin < rmax
    if (z_planes.size() < 2) {
        G4cerr << "Error in polyhedra " << name << ": need at least 2 z-planes, found " 
               << z_planes.size() << G4endl;
        exit(1);
    }
    
    for (size_t i = 1; i < z_planes.size(); i++) {
        if (z_planes[i] <= z_planes[i-1]) {
            G4cerr << "Error in polyhedra " << name << ": z-planes must be in ascending order. "
                   << "Found z[" << i-1 << "] = " << z_planes[i-1]/mm << " mm >= z[" 
                   << i << "] = " << z_planes[i]/mm << " mm" << G4endl;
            exit(1);
        }
    }
    
    for (size_t i = 0; i < z_planes.size(); i++) {
        if (rmin[i] >= rmax[i]) {
            G4cerr << "Error in polyhedra " << name << ": rmin must be less than rmax. "
                   << "Found at z[" << i << "] = " << z_planes[i]/mm << " mm: rmin = " 
                   << rmin[i]/mm << " mm >= rmax = " << rmax[i]/mm << " mm" << G4endl;
            exit(1);
        }
    }
    
    // Create the polyhedra solid
    try {
        return new G4Polyhedra(name, sphi, dphi, numSides, z_planes.size(), 
                              z_planes.data(), rmin.data(), rmax.data());
    } catch (const std::exception& e) {
        G4cerr << "Error creating polyhedra " << name << ": " << e.what() << G4endl;
        G4cerr << "Z planes: ";
        for (size_t i = 0; i < z_planes.size(); i++) {
            G4cerr << z_planes[i]/mm << " ";
        }
        G4cerr << "\nRmin: ";
        for (size_t i = 0; i < rmin.size(); i++) {
            G4cerr << rmin[i]/mm << " ";
        }
        G4cerr << "\nRmax: ";
        for (size_t i = 0; i < rmax.size(); i++) {
            G4cerr << rmax[i]/mm << " ";
        }
        G4cerr << G4endl;
        exit(1);
    }
}
