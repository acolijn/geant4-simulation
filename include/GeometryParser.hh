#ifndef GeometryParser_h
#define GeometryParser_h 1

#include "json.hpp"
#include "G4Material.hh"
#include "G4VPhysicalVolume.hh"
#include "G4LogicalVolume.hh"
#include "G4ThreeVector.hh"
#include "G4RotationMatrix.hh"
#include "G4VSolid.hh"
#include "G4VSensitiveDetector.hh"
#include "G4AssemblyVolume.hh"
#include <string>
#include <map>
#include <vector>
#include <memory>

using json = nlohmann::json;

/**
 * @class GeometryParser
 * @brief Parses JSON configuration files for geometry and materials
 *
 * This class handles the parsing of external JSON configuration files
 * for both detector geometry and materials. It provides methods to:
 * - Load and parse JSON files
 * - Create G4 materials from JSON descriptions
 * - Create G4 volumes from JSON descriptions
 */
class GeometryParser {
public:
    /** @brief Constructor */
    GeometryParser();
    
    /** @brief Destructor */
    ~GeometryParser();

    /**
     * @brief Load geometry configuration from JSON file
     * @param filename Path to geometry JSON file
     */
    void LoadGeometryConfig(const std::string& filename);

    /**
     * @brief Load materials configuration from JSON file
     * @param filename Path to materials JSON file
     */
    void LoadMaterialsConfig(const std::string& filename);

    /**
     * @brief Create the world volume from loaded configuration
     * @return Pointer to the physical world volume
     */
    G4VPhysicalVolume* ConstructGeometry();
    
    /**
     * @brief Setup sensitive detectors for active volumes
     * @details This method assigns sensitive detectors to volumes marked as active in the JSON config
     */
    void SetupSensitiveDetectors();

private:
    json geometryConfig;    ///< Geometry configuration
    json materialsConfig;   ///< Materials configuration
    
    std::map<std::string, G4Material*> materials;        ///< Cache of created materials
    std::map<std::string, G4LogicalVolume*> volumes;    ///< Cache of created volumes
    std::map<std::string, G4LogicalVolume*> logicalVolumeMap;  ///< Map of logical volumes by name
    std::map<std::string, G4VSolid*> solids;           ///< Cache of created solids
    std::map<std::string, G4AssemblyVolume*> assemblies; ///< Cache of created assemblies
    std::string configPath;                           ///< Path to the configuration files

    /**
     * @brief Create a G4Material from JSON configuration
     * @param name Material name
     * @param config JSON configuration for the material
     * @return Pointer to created G4Material
     */
    G4Material* CreateMaterial(const std::string& name, const json& config);

    /**
     * @brief Create a G4LogicalVolume from JSON configuration
     * @param config JSON configuration for the volume
     * @return Pointer to created G4LogicalVolume
     */
    G4LogicalVolume* CreateVolume(const json& config);
    
    /**
     * @brief Create a G4VSolid from JSON configuration
     * @param config JSON configuration for the solid
     * @param name Name for the solid
     * @return Pointer to created G4VSolid
     */
    G4VSolid* CreateSolid(const json& config, const std::string& name);
    
    /**
     * @brief Create a box solid from JSON configuration
     * @param dims JSON object containing box dimensions
     * @param name Name for the solid
     * @return Pointer to created G4Box
     */
    G4VSolid* CreateBoxSolid(const json& dims, const std::string& name);
    
    /**
     * @brief Create a sphere solid from JSON configuration
     * @param dims JSON object containing sphere dimensions
     * @param name Name for the solid
     * @return Pointer to created G4Sphere
     */
    G4VSolid* CreateSphereSolid(const json& dims, const std::string& name);
    
    /**
     * @brief Create a cylinder/tube solid from JSON configuration
     * @param dims JSON object containing cylinder dimensions
     * @param name Name for the solid
     * @return Pointer to created G4Tubs
     */
    G4VSolid* CreateCylinderSolid(const json& dims, const std::string& name);
    
    /**
     * @brief Create a cone solid from JSON configuration
     * @param dims JSON object containing cone dimensions
     * @param name Name for the solid
     * @return Pointer to created G4Cons
     */
    G4VSolid* CreateConeSolid(const json& dims, const std::string& name);
    
    /**
     * @brief Create a trapezoid solid from JSON configuration
     * @param dims JSON object containing trapezoid dimensions
     * @param name Name for the solid
     * @return Pointer to created G4Trd
     */
    G4VSolid* CreateTrapezoidSolid(const json& dims, const std::string& name);
    
    /**
     * @brief Create a torus solid from JSON configuration
     * @param dims JSON object containing torus dimensions
     * @param name Name for the solid
     * @return Pointer to created G4Torus
     */
    G4VSolid* CreateTorusSolid(const json& dims, const std::string& name);
    
    /**
     * @brief Create an ellipsoid solid from JSON configuration
     * @param dims JSON object containing ellipsoid dimensions
     * @param name Name for the solid
     * @return Pointer to created G4Ellipsoid
     */
    G4VSolid* CreateEllipsoidSolid(const json& dims, const std::string& name);
    
    /**
     * @brief Create an orb solid from JSON configuration
     * @param dims JSON object containing orb dimensions
     * @param name Name for the solid
     * @return Pointer to created G4Orb
     */
    G4VSolid* CreateOrbSolid(const json& dims, const std::string& name);
    
    /**
     * @brief Create an elliptical tube solid from JSON configuration
     * @param dims JSON object containing elliptical tube dimensions
     * @param name Name for the solid
     * @return Pointer to created G4EllipticalTube
     */
    G4VSolid* CreateEllipticalTubeSolid(const json& dims, const std::string& name);
    
    /**
     * @brief Create a polycone solid from JSON configuration
     * @param config JSON configuration for the polycone (needs both dims and possibly config)
     * @param dims JSON object containing polycone dimensions
     * @param name Name for the solid
     * @return Pointer to created G4Polycone
     */
    G4VSolid* CreatePolyconeSolid(const json& config, const json& dims, const std::string& name);
    
    /**
     * @brief Create a polyhedra solid from JSON configuration
     * @param config JSON configuration for the polyhedra (needs both dims and possibly config)
     * @param dims JSON object containing polyhedra dimensions
     * @param name Name for the solid
     * @return Pointer to created G4VSolid
     */
    G4VSolid* CreatePolyhedraSolid(const json& config, const json& dims, const std::string& name);
    
    /**
     * @brief Create a boolean solid (union, subtraction, intersection)
     * @param config JSON configuration for the boolean operation
     * @param name Name for the resulting solid
     * @return Pointer to created G4VSolid
     */
    G4VSolid* CreateBooleanSolid(const json& config, const std::string& name);
    
    /**
     * @brief Load and parse an external JSON geometry file
     * @param filename Path to the external JSON file
     * @return JSON object containing the parsed file
     */
    json LoadExternalGeometry(const std::string& filename);
    
    /**
     * @brief Create an assembly from JSON configuration
     * @param config JSON configuration for the assembly
     */
    void CreateAssembly(const json& config);
    
    /**
     * @brief Import an assembled geometry from an external file
     * @param config JSON configuration for the import
     * @param motherVolume Logical volume to place the imported geometry in
     */
    void ImportAssembledGeometry(const json& config, G4LogicalVolume* motherVolume);

    /**
     * @brief Convert JSON vector to G4ThreeVector with units
     * @param vec JSON object containing x,y,z and unit
     * @return G4ThreeVector with applied units
     */
    G4ThreeVector ParseVector(const json& vec);

    /**
     * @brief Convert JSON rotation to G4RotationMatrix
     * @param rot JSON object containing rotation angles
     * @return Pointer to new G4RotationMatrix
     */
    G4RotationMatrix* ParseRotation(const json& rot);
    
    /**
     * @brief Parse a placement object and extract position and rotation
     * @param config JSON object containing placement information
     * @param position Reference to G4ThreeVector to store position
     * @param rotation Reference to G4RotationMatrix pointer to store rotation
     * @details Handles the geometry-editor format with placements array
     */
    void ParsePlacement(const json& config, G4ThreeVector& position, G4RotationMatrix*& rotation);
};

#endif
