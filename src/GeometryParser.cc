#include "GeometryParser.hh"
#include "G4NistManager.hh"

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
void GeometryParser::LoadMaterialsConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open materials config file: " + filename);
    }
    file >> materialsConfig;
}

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
    std::cout << "Material config: " << config << std::endl;
    
    G4Material* material = nullptr;
    std::string type = config["type"].get<std::string>();

    if (type == "nist") {
        // Get material from NIST database
        std::cout << "NIST material: " << config["name"] << std::endl;
        G4NistManager* nist = G4NistManager::Instance();
        material = nist->FindOrBuildMaterial(config["name"].get<std::string>());
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
 * @param vec JSON object containing x,y,z components and unit
 * @return G4ThreeVector with components scaled to proper units
 * @details Expects JSON format: {"x": val, "y": val, "z": val, "unit": "mm|cm|m"}
 */
G4ThreeVector GeometryParser::ParseVector(const json& vec) {
    G4double x = vec["x"].get<double>();
    G4double y = vec["y"].get<double>();
    G4double z = vec["z"].get<double>();
    
    std::string unit = vec["unit"].get<std::string>();
    G4double scale = 1.0;
    if (unit == "mm") scale = mm;
    else if (unit == "cm") scale = cm;
    else if (unit == "m") scale = m;
    
    return G4ThreeVector(x*scale, y*scale, z*scale);
}

/**
 * @brief Convert JSON rotation angles to G4RotationMatrix
 * @param rot JSON object containing rotation angles
 * @return Pointer to new G4RotationMatrix
 * @details Expects JSON format: {"x": angle_x, "y": angle_y, "z": angle_z}
 *          Angles should be in degrees
 *          Rotations are applied in the Geant4 sequence: first X, then Y, then Z
 */
G4RotationMatrix* GeometryParser::ParseRotation(const json& rot) {
    G4double rx = rot["x"].get<double>();
    G4double ry = rot["y"].get<double>();
    G4double rz = rot["z"].get<double>();
    
    std::string unit = rot["unit"].get<std::string>();
    G4double scale = 1.0;
    if (unit == "deg") scale = deg;
    else if (unit == "rad") scale = rad;
    
    // Create a rotation matrix using individual rotations around each axis
    // This ensures rotations are applied in the correct sequence: X, then Y, then Z
    G4RotationMatrix* rotMatrix = new G4RotationMatrix();
    
    // Apply rotations in sequence (Geant4 uses active rotations)
    rotMatrix->rotateX(rx*scale);
    rotMatrix->rotateY(ry*scale);
    rotMatrix->rotateZ(rz*scale);
    
    return rotMatrix;
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
    std::cout << "Creating volume: " << config["name"] << std::endl;
    std::string name = config["name"].get<std::string>();
    
    // Check if volume already exists
    if (volumes.find(name) != volumes.end()) {
        return volumes[name];
    }

    // Get material
    std::string mat_name = config["material"].get<std::string>();
    G4Material* material = nullptr;
    if (materials.find(mat_name) == materials.end()) {
        material = CreateMaterial(mat_name, materialsConfig["materials"][mat_name]);
    } else {
        material = materials[mat_name];
    }

    // Create solid
    G4VSolid* solid = CreateSolid(config, name);

    // Create logical volume
    G4LogicalVolume* logicalVolume = new G4LogicalVolume(solid, material, name);
    volumes[name] = logicalVolume;
    
    std::cout << "Created volume: " << name << std::endl;
    return logicalVolume;
}

/**
 * @brief Construct the complete detector geometry
 * @return Pointer to the world physical volume
 * @details Creates the world volume and places all other volumes within it
 *          according to the loaded geometry configuration
 */
G4VPhysicalVolume* GeometryParser::ConstructGeometry() {
    // Create world volume
    G4LogicalVolume* worldLV = CreateVolume(geometryConfig["world"]);
    G4VPhysicalVolume* worldPV = new G4PVPlacement(
        nullptr, G4ThreeVector(), worldLV, "World", nullptr, false, 0);

    // Create other volumes
    for (const auto& volConfig : geometryConfig["volumes"]) {
        // Check if this is an external geometry import
        if (volConfig.contains("external_file")) {
            std::string motherName = volConfig["mother_volume"].get<std::string>();
            G4LogicalVolume* motherVolume = volumes[motherName];
            ImportAssembledGeometry(volConfig, motherVolume);
            continue;
        }

        std::cout << "Creating volume: " << volConfig["name"] << std::endl;
        
        G4LogicalVolume* logicalVolume = CreateVolume(volConfig);
        std::cout << "Created volume: " << volConfig["name"] << std::endl;
        
        std::cout << "Placing volume: " << volConfig["name"] << std::endl;
        G4ThreeVector position = ParseVector(volConfig["position"]);
        G4RotationMatrix* rotation = nullptr;
        std::cout << "Position: " << position << std::endl;
        
        if (volConfig.contains("rotation")) {
            rotation = ParseRotation(volConfig["rotation"]);
            std::cout << "Rotation: " << rotation << std::endl;
        }
        
        std::string motherName = volConfig["mother_volume"].get<std::string>();
        G4LogicalVolume* motherVolume = volumes[motherName];
        std::cout << "Mother volume: " << motherName << std::endl;
        
        // Check for multiple copies (replicas or parameterized volumes)
        if (volConfig.contains("copies") && volConfig["copies"].get<int>() > 1) {
            int copies = volConfig["copies"].get<int>();
            std::cout << "Number of copies: " << copies << std::endl;
            
            // Simple placement for multiple copies
            for (int i = 0; i < copies; i++) {
                G4ThreeVector pos = position;
                
                // Apply offset if specified
                if (volConfig.contains("copy_offset")) {
                    G4ThreeVector offset = ParseVector(volConfig["copy_offset"]);
                    pos += offset * i;
                }
                
                new G4PVPlacement(rotation, pos, logicalVolume,
                                volConfig["name"].get<std::string>() + "_" + std::to_string(i),
                                motherVolume, false, i);
            }
        } else {
            // Single placement
            std::cout << "Placing single volume: " << volConfig["name"] << std::endl;
            new G4PVPlacement(rotation, position, logicalVolume,
                            volConfig["name"].get<std::string>(),
                            motherVolume, false, 0);
        }
    }

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
    if (solids.find(name) != solids.end()) {
        return solids[name];
    }
    
    G4VSolid* solid = nullptr;
    std::string type = config["type"].get<std::string>();
    
    // Handle boolean operations
    if (type == "union" || type == "subtraction" || type == "intersection") {
        solid = CreateBooleanSolid(config, name);
    }
    // Basic shapes
    else if (type == "box") {
        G4ThreeVector size = ParseVector(config["size"]);
        solid = new G4Box(name, size.x()/2, size.y()/2, size.z()/2);
    }
    else if (type == "sphere") {
        G4double rmin = 0;
        G4double rmax = config["radius"].get<double>();
        G4double sphi = 0;
        G4double dphi = 360 * deg;
        G4double stheta = 0;
        G4double dtheta = 180 * deg;
        
        // Apply unit scaling for radius
        G4double lengthScale = mm; // Default to mm if no unit specified
        if (config.contains("unit")) {
            std::string unit = config["unit"].get<std::string>();
            if (unit == "mm") lengthScale = mm;
            else if (unit == "cm") lengthScale = cm;
            else if (unit == "m") lengthScale = m;
        }
        
        rmax *= lengthScale;
        
        if (config.contains("inner_radius")) {
            rmin = config["inner_radius"].get<double>() * lengthScale;
        }
        if (config.contains("start_phi")) {
            sphi = config["start_phi"].get<double>() * deg;
        }
        if (config.contains("delta_phi")) {
            dphi = config["delta_phi"].get<double>() * deg;
        }
        if (config.contains("start_theta")) {
            stheta = config["start_theta"].get<double>() * deg;
        }
        if (config.contains("delta_theta")) {
            dtheta = config["delta_theta"].get<double>() * deg;
        }
        
        solid = new G4Sphere(name, rmin, rmax, sphi, dphi, stheta, dtheta);
    }
    else if (type == "cylinder" || type == "tube") {
        G4double rmin = 0;
        G4double rmax = config["radius"].get<double>();
        G4double hz = config["height"].get<double>() / 2; // Half-height for G4Tubs
        G4double sphi = 0;
        G4double dphi = 360 * deg;
        
        // Apply unit scaling for dimensions
        G4double lengthScale = mm; // Default to mm if no unit specified
        if (config.contains("unit")) {
            std::string unit = config["unit"].get<std::string>();
            if (unit == "mm") lengthScale = mm;
            else if (unit == "cm") lengthScale = cm;
            else if (unit == "m") lengthScale = m;
        }
        
        rmax *= lengthScale;
        hz *= lengthScale;
        
        if (config.contains("inner_radius")) {
            rmin = config["inner_radius"].get<double>() * lengthScale;
        } else if (config.contains("innerRadius")) {
            // Support both naming conventions
            rmin = config["innerRadius"].get<double>() * lengthScale;
        }
        if (config.contains("start_phi")) {
            sphi = config["start_phi"].get<double>() * deg;
        }
        if (config.contains("delta_phi")) {
            dphi = config["delta_phi"].get<double>() * deg;
        }
        
        solid = new G4Tubs(name, rmin, rmax, hz, sphi, dphi);
    }
    else if (type == "cone") {
        G4double rmin1 = 0;
        G4double rmax1 = config["radius1"].get<double>() * mm;
        G4double rmin2 = 0;
        G4double rmax2 = config["radius2"].get<double>() * mm;
        G4double hz = config["height"].get<double>() * mm / 2;
        G4double sphi = 0;
        G4double dphi = 360 * deg;
        
        if (config.contains("inner_radius1")) {
            rmin1 = config["inner_radius1"].get<double>() * mm;
        }
        if (config.contains("inner_radius2")) {
            rmin2 = config["inner_radius2"].get<double>() * mm;
        }
        if (config.contains("start_phi")) {
            sphi = config["start_phi"].get<double>() * deg;
        }
        if (config.contains("delta_phi")) {
            dphi = config["delta_phi"].get<double>() * deg;
        }
        
        solid = new G4Cons(name, rmin1, rmax1, rmin2, rmax2, hz, sphi, dphi);
    }
    else if (type == "trd") {
        G4double x1 = config["x1"].get<double>() * mm / 2;
        G4double x2 = config["x2"].get<double>() * mm / 2;
        G4double y1 = config["y1"].get<double>() * mm / 2;
        G4double y2 = config["y2"].get<double>() * mm / 2;
        G4double hz = config["height"].get<double>() * mm / 2;
        
        solid = new G4Trd(name, x1, x2, y1, y2, hz);
    }
    else if (type == "torus") {
        G4double rmin = 0;
        G4double rmax = config["tube_radius"].get<double>() * mm;
        G4double rtor = config["torus_radius"].get<double>() * mm;
        G4double sphi = 0;
        G4double dphi = 360 * deg;
        
        if (config.contains("inner_radius")) {
            rmin = config["inner_radius"].get<double>() * mm;
        }
        if (config.contains("start_phi")) {
            sphi = config["start_phi"].get<double>() * deg;
        }
        if (config.contains("delta_phi")) {
            dphi = config["delta_phi"].get<double>() * deg;
        }
        
        solid = new G4Torus(name, rmin, rmax, rtor, sphi, dphi);
    }
    else if (type == "ellipsoid") {
        G4double ax = config["ax"].get<double>() * mm;
        G4double by = config["by"].get<double>() * mm;
        G4double cz = config["cz"].get<double>() * mm;
        G4double zcut1 = -cz;
        G4double zcut2 = cz;
        
        if (config.contains("zcut1")) {
            zcut1 = config["zcut1"].get<double>() * mm;
        }
        if (config.contains("zcut2")) {
            zcut2 = config["zcut2"].get<double>() * mm;
        }
        
        solid = new G4Ellipsoid(name, ax, by, cz, zcut1, zcut2);
    }
    else if (type == "orb") {
        G4double radius = config["radius"].get<double>() * mm;
        solid = new G4Orb(name, radius);
    }
    else if (type == "elliptical_tube") {
        G4double dx = config["dx"].get<double>() * mm;
        G4double dy = config["dy"].get<double>() * mm;
        G4double dz = config["dz"].get<double>() * mm;
        
        solid = new G4EllipticalTube(name, dx, dy, dz);
    }
    else if (type == "polycone") {
        G4double sphi = 0;
        G4double dphi = 360 * deg;
        
        if (config.contains("start_phi")) {
            sphi = config["start_phi"].get<double>() * deg;
        }
        if (config.contains("delta_phi")) {
            dphi = config["delta_phi"].get<double>() * deg;
        }
        
        // Get z planes and radii
        std::vector<G4double> z_planes;
        std::vector<G4double> rmin;
        std::vector<G4double> rmax;
        
        for (const auto& plane : config["planes"]) {
            z_planes.push_back(plane["z"].get<double>() * mm);
            rmin.push_back(plane.contains("rmin") ? plane["rmin"].get<double>() * mm : 0);
            rmax.push_back(plane["rmax"].get<double>() * mm);
        }
        
        solid = new G4Polycone(name, sphi, dphi, z_planes.size(), 
                            z_planes.data(), rmin.data(), rmax.data());
    }
    else if (type == "polyhedra") {
        G4double sphi = 0;
        G4double dphi = 360 * deg;
        G4int numSides = config["num_sides"].get<int>();
        
        if (config.contains("start_phi")) {
            sphi = config["start_phi"].get<double>() * deg;
        }
        if (config.contains("delta_phi")) {
            dphi = config["delta_phi"].get<double>() * deg;
        }
        
        // Get z planes and radii
        std::vector<G4double> z_planes;
        std::vector<G4double> rmin;
        std::vector<G4double> rmax;
        
        for (const auto& plane : config["planes"]) {
            z_planes.push_back(plane["z"].get<double>() * mm);
            rmin.push_back(plane.contains("rmin") ? plane["rmin"].get<double>() * mm : 0);
            rmax.push_back(plane["rmax"].get<double>() * mm);
        }
        
        solid = new G4Polyhedra(name, sphi, dphi, numSides, z_planes.size(), 
                              z_planes.data(), rmin.data(), rmax.data());
    }
    else {
        throw std::runtime_error("Unsupported solid type: " + type);
    }
    
    // Cache the solid
    solids[name] = solid;
    return solid;
}

/**
 * @brief Create a boolean solid (union, subtraction, intersection)
 * @param config JSON configuration for the boolean operation
 * @param name Name for the resulting solid
 * @return Pointer to created G4VSolid
 * @throws std::runtime_error if boolean operation fails
 * @details Creates a boolean solid by combining two other solids.
 *          The first solid is the base, and the second is applied to it.
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
    
    // Check for relative_position first, then fall back to position for backward compatibility
    if (config.contains("relative_position")) {
        position = ParseVector(config["relative_position"]);
    } else if (config.contains("position") && !config.contains("mother_volume")) {
        // Only use position if this is not a volume placement (no mother_volume)
        position = ParseVector(config["position"]);
    }
    
    if (config.contains("rotation") && !config.contains("mother_volume")) {
        rotation = ParseRotation(config["rotation"]);
    } else if (config.contains("relative_rotation")) {
        rotation = ParseRotation(config["relative_rotation"]);
    }
    
    // Create the boolean solid
    G4VSolid* booleanSolid = nullptr;
    
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
void GeometryParser::ImportAssembledGeometry(const json& config, G4LogicalVolume* parentVolume) {
    // Load the external file
    std::string filename = config["external_file"].get<std::string>();
    json externalConfig = LoadExternalGeometry(filename);
    
    // Get transformation for the assembly
    G4ThreeVector position(0, 0, 0);
    G4RotationMatrix* rotation = nullptr;
    
    if (config.contains("position")) {
        position = ParseVector(config["position"]);
    }
    
    if (config.contains("rotation")) {
        rotation = ParseRotation(config["rotation"]);
    }
    
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
        externalVolumes[name] = logicalVolume;
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
            
            G4ThreeVector volPosition = ParseVector(volConfig["position"]);
            G4RotationMatrix* volRotation = nullptr;
            
            if (volConfig.contains("rotation")) {
                volRotation = ParseRotation(volConfig["rotation"]);
            }
            
            new G4PVPlacement(volRotation, volPosition, logicalVolume, name, motherVolume, false, 0);
        }
    }
}
