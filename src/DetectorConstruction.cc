/**
 * @file DetectorConstruction.cc
 * @brief Implementation of the DetectorConstruction class
 */

#include "DetectorConstruction.hh"
#include "G4SystemOfUnits.hh"
#include "G4UIdirectory.hh"
#include "G4RunManager.hh"

/**
 * @brief Nested messenger class for DetectorConstruction
 * 
 * Handles UI commands for changing geometry and material files at runtime
 */
class DetectorConstruction::DetectorMessenger : public G4UImessenger
{
  public:
    DetectorMessenger(DetectorConstruction* detector);
    virtual ~DetectorMessenger();
    
    virtual void SetNewValue(G4UIcommand* command, G4String newValue);
    
  private:
    DetectorConstruction* fDetector;
    
    G4UIdirectory*        fDetectorDir;
    G4UIcmdWithAString*   fGeometryFileCmd;
    G4UIcmdWithAString*   fMaterialsFileCmd;
    G4UIcommand*          fRebuildCmd;
};

/**
 * @brief Constructor for the DetectorMessenger
 * @param detector Pointer to the associated DetectorConstruction
 */
DetectorConstruction::DetectorMessenger::DetectorMessenger(DetectorConstruction* detector)
: fDetector(detector)
{
    fDetectorDir = new G4UIdirectory("/detector/");
    fDetectorDir->SetGuidance("Detector configuration commands");
    
    fGeometryFileCmd = new G4UIcmdWithAString("/detector/setGeometryFile", this);
    fGeometryFileCmd->SetGuidance("Set the path to the geometry configuration JSON file");
    fGeometryFileCmd->SetParameterName("GeometryFile", false);
    
    fMaterialsFileCmd = new G4UIcmdWithAString("/detector/setMaterialsFile", this);
    fMaterialsFileCmd->SetGuidance("Set the path to the materials configuration JSON file");
    fMaterialsFileCmd->SetParameterName("MaterialsFile", false);
    
    fRebuildCmd = new G4UIcommand("/detector/rebuild", this);
    fRebuildCmd->SetGuidance("Rebuild the geometry with the current configuration files");
}

/**
 * @brief Destructor for the DetectorMessenger
 */
DetectorConstruction::DetectorMessenger::~DetectorMessenger()
{
    delete fGeometryFileCmd;
    delete fMaterialsFileCmd;
    delete fRebuildCmd;
    delete fDetectorDir;
}

/**
 * @brief Handle UI commands
 * @param command The command being executed
 * @param newValue The new value for the command parameter
 */
void DetectorConstruction::DetectorMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
    if (command == fGeometryFileCmd) {
        fDetector->SetGeometryFile(newValue);
        //fDetector->RebuildGeometry();
    } else if (command == fMaterialsFileCmd) {
        fDetector->SetMaterialsFile(newValue);
        //fDetector->RebuildGeometry();
    } else if (command == fRebuildCmd) {
        fDetector->RebuildGeometry();
    }
}

/**
 * @brief Constructor implementation
 * @param geomFile Path to geometry configuration file
 * @param matFile Path to materials configuration file
 *
 * Initializes the geometry parser with the specified configuration files.
 */
DetectorConstruction::DetectorConstruction(const std::string& geomFile,
                                         const std::string& matFile)
: G4VUserDetectorConstruction(),
  fMessenger(nullptr),
  geometryFile(geomFile),
  materialsFile(matFile),
  lXeVolume(nullptr)
{
    // Create the messenger for this class
    fMessenger = new DetectorMessenger(this);
}

/**
 * @brief Destructor implementation
 * 
 * Cleans up the messenger object.
 */
DetectorConstruction::~DetectorConstruction()
{
    delete fMessenger;
}

/**
 * @brief Constructs the detector geometry from JSON configuration
 * @return Pointer to the world physical volume
 *
 * This method:
 * 1. Loads geometry and materials configurations
 * 2. Constructs all volumes using the GeometryParser
 * 3. Stores a pointer to the LXe volume for scoring
 *
 * The complete geometry specification is read from the JSON files,
 * allowing for easy modification without recompiling.
 */
G4VPhysicalVolume* DetectorConstruction::Construct()
{
    // Load configuration files
    // print the file names:
    G4cout << "Geometry file: " << geometryFile << G4endl;
    G4cout << "Materials file: " << materialsFile << G4endl;

    parser.LoadMaterialsConfig(materialsFile);
    parser.LoadGeometryConfig(geometryFile);

    // Construct the geometry
    G4VPhysicalVolume* worldPhys = parser.ConstructGeometry();

    // Get pointer to LXe volume for scoring
    // The LXe volume should be the first daughter of the world volume
    G4LogicalVolume* worldLV = worldPhys->GetLogicalVolume();
    if (worldLV->GetNoDaughters() > 0) {
        G4VPhysicalVolume* lxePhys = worldLV->GetDaughter(0);
        lXeVolume = lxePhys->GetLogicalVolume();
    }

    return worldPhys;
}

/**
 * @brief Set the geometry configuration file path
 * @param path Path to the geometry JSON file
 */
void DetectorConstruction::SetGeometryFile(const G4String& path)
{
    geometryFile = path;
    G4cout << "Geometry file set to: " << path << G4endl;
}

/**
 * @brief Set the materials configuration file path
 * @param path Path to the materials JSON file
 */
void DetectorConstruction::SetMaterialsFile(const G4String& path)
{
    materialsFile = path;
    G4cout << "Materials file set to: " << path << G4endl;
}

/**
 * @brief Rebuild the geometry with the current configuration files
 * @return true if rebuild was successful, false otherwise
 */
G4bool DetectorConstruction::RebuildGeometry()
{
    G4cout << "Rebuilding geometry with:" << G4endl;
    G4cout << "  Geometry file: " << geometryFile << G4endl;
    G4cout << "  Materials file: " << materialsFile << G4endl;
    
    // Clear any existing volumes and materials
    parser = GeometryParser();
    lXeVolume = nullptr;
    
    // This will trigger Geant4 to call Construct() again
    G4RunManager::GetRunManager()->ReinitializeGeometry();
    G4RunManager::GetRunManager()->GeometryHasBeenModified();
    
    return true;
}
