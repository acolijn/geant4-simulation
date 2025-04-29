#ifndef DetectorConstruction_h
#define DetectorConstruction_h 1

#include "G4VUserDetectorConstruction.hh"
#include "G4LogicalVolume.hh"
#include "GeometryParser.hh"
#include "G4UImessenger.hh"
#include "G4UIcmdWithAString.hh"
#include <string>

/**
 * @class DetectorConstruction
 * @brief Constructs the detector geometry from JSON configuration files
 *
 * This class reads geometry and material definitions from JSON files
 * and constructs the complete detector setup.
 */
class DetectorConstruction : public G4VUserDetectorConstruction
{
  public:
    /**
     * @brief Constructor
     * @param geomFile Path to geometry configuration file
     * @param matFile Path to materials configuration file
     */
    DetectorConstruction(const std::string& geomFile="config/dummy1.json",
                         const std::string& matFile="config/dummy2.json");
    
    /** @brief Destructor */
    virtual ~DetectorConstruction();

    /**
     * @brief Construct detector geometry from configuration files
     * @return Pointer to the world physical volume
     */
    virtual G4VPhysicalVolume* Construct();

    /**
     * @brief Set the geometry configuration file path
     * @param path Path to the geometry JSON file
     */
    void SetGeometryFile(const G4String& path);
    
    /**
     * @brief Set the materials configuration file path
     * @param path Path to the materials JSON file
     */
    void SetMaterialsFile(const G4String& path);
    
    /**
     * @brief Get the current geometry file path
     * @return Current geometry file path
     */
    G4String GetGeometryFile() const { return geometryFile; }
    
    /**
     * @brief Get the current materials file path
     * @return Current materials file path
     */
    G4String GetMaterialsFile() const { return materialsFile; }
    
    /**
     * @brief Rebuild the geometry with the current configuration files
     * @return true if rebuild was successful, false otherwise
     */
    G4bool RebuildGeometry();

  private:
    class DetectorMessenger;
    DetectorMessenger* fMessenger;   ///< Messenger for UI commands
    
    GeometryParser parser;           ///< Parser for JSON configuration
    std::string geometryFile;        ///< Path to geometry config file
    std::string materialsFile;       ///< Path to materials config file
    G4LogicalVolume* lXeVolume;      ///< Pointer to LXe volume for scoring
};

#endif
