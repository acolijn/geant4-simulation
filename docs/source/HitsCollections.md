# Working with Hits Collections in Geant4

This guide explains how to use the hits collection system in the Geant4 Geometry Editor and how to extend it with custom hits collections in your Geant4 simulation.

## 1. Using Active Volumes in the Geometry Editor

### Marking Volumes as Active

1. Select a volume in the geometry tree
2. In the properties panel, check the "Active Volume (for hit collection)" checkbox
3. By default, the volume will be added to the "MyHitsCollection"

![Active Volume Toggle](images/active_volume_toggle.png)

### Hits Collection Selection

For each active volume, you can select which hits collection it belongs to:

1. Mark a volume as active by checking the "Active Volume" checkbox
2. Use the "Hits Collection" dropdown to select the collection
3. By default, "MyHitsCollection" is selected

### JSON Export Format

When you export your geometry, active volumes and hits collections will be included in the JSON:

```json
{
  "world": { /* world properties */ },
  "volumes": [
    {
      "name": "Detector1",
      "type": "box",
      /* other properties */
      "isActive": true,
      "hitsCollectionName": "MyHitsCollection"
    }
  ],
  "hitsCollections": [
    {
      "name": "MyHitsCollection",
      "description": "Default hits collection for energy deposits",
      "associatedVolumes": ["Detector1", "Detector2"]
    }
  ]
}
```

## 2. Understanding the Default Implementation

The simulation comes with a default `MyHitsCollection` implementation that demonstrates how to:
- Define a hit class
- Create a sensitive detector
- Process hits and store them in a collection

### Key Files

- `MyHit.hh/cc`: Defines what information is stored for each hit
- `MySensitiveDetector.hh/cc`: Processes physics steps and creates hits
- `GeometryParser.cc`: Assigns sensitive detectors to active volumes

## 3. Creating Your Own Hits Collection

To create a new hits collection (e.g., "ScintillatorHits"):

### Step 1: Create a new hit class

```cpp
// ScintillatorHit.hh
#ifndef ScintillatorHit_h
#define ScintillatorHit_h 1

#include "G4VHit.hh"
#include "G4THitsCollection.hh"
#include "G4ThreeVector.hh"
#include "G4Threading.hh"

class ScintillatorHit : public G4VHit {
public:
    ScintillatorHit();
    virtual ~ScintillatorHit();
    
    // Add your hit properties and methods here
    void SetEnergy(G4double e) { fEnergy = e; }
    void SetPosition(G4ThreeVector pos) { fPosition = pos; }
    void SetScintillationPhotons(G4int n) { fScintPhotons = n; }
    
    G4double GetEnergy() const { return fEnergy; }
    G4ThreeVector GetPosition() const { return fPosition; }
    G4int GetScintillationPhotons() const { return fScintPhotons; }
    
private:
    G4double fEnergy;
    G4ThreeVector fPosition;
    G4int fScintPhotons;  // Scintillator-specific property
};

typedef G4THitsCollection<ScintillatorHit> ScintillatorHitsCollection;

#endif
```

### Step 2: Create a new sensitive detector

```cpp
// ScintillatorSD.hh
#ifndef ScintillatorSD_h
#define ScintillatorSD_h 1

#include "G4VSensitiveDetector.hh"
#include "ScintillatorHit.hh"

class ScintillatorSD : public G4VSensitiveDetector {
public:
    ScintillatorSD(const G4String& name);
    virtual ~ScintillatorSD();
    
    virtual void Initialize(G4HCofThisEvent* hitCollection);
    virtual G4bool ProcessHits(G4Step* step, G4TouchableHistory* history);
    virtual void EndOfEvent(G4HCofThisEvent* hitCollection);
    
private:
    ScintillatorHitsCollection* fHitsCollection;
    G4int fHitsCollectionID;
};

#endif
```

### Step 3: Modify GeometryParser to support your new collection

Add to the `SetupSensitiveDetectors` method:

```cpp
void GeometryParser::SetupSensitiveDetectors() {
    // Existing code...
    
    // Create and register the default sensitive detector
    MySensitiveDetector* mySD = new MySensitiveDetector("MySD");
    sdManager->AddNewDetector(mySD);
    
    // Create and register your new sensitive detector
    ScintillatorSD* scintSD = new ScintillatorSD("ScintillatorSD");
    sdManager->AddNewDetector(scintSD);
    
    // Iterate through all volumes in the JSON configuration
    for (const auto& volConfig : geometryConfig["volumes"]) {
        // Check if this volume is marked as active
        if (volConfig.contains("isActive") && volConfig["isActive"].get<bool>()) {
            // Get the hits collection name
            std::string hitsCollName = "MyHitsCollection"; // Default
            if (volConfig.contains("hitsCollectionName")) {
                hitsCollName = volConfig["hitsCollectionName"].get<std::string>();
            }
            
            // Get the logical volume
            std::string volName = volConfig["name"].get<std::string>();
            G4LogicalVolume* logicalVol = logicalVolumeMap[volName + "_logical"];
            
            if (logicalVol) {
                // Assign the appropriate sensitive detector based on the collection name
                if (hitsCollName == "MyHitsCollection") {
                    logicalVol->SetSensitiveDetector(mySD);
                }
                else if (hitsCollName == "ScintillatorHits") {
                    logicalVol->SetSensitiveDetector(scintSD);
                }
            }
        }
    }
}
```

## 4. Accessing Hits in the Analysis

To access your hits in the analysis code:

```cpp
void AnalysisManager::ProcessEvent(const G4Event* event) {
    // Get hits collections IDs
    G4HCofThisEvent* hce = event->GetHCofThisEvent();
    if (!hce) return;
    
    // Get the collection ID (do this once and cache it)
    static G4int myHitsID = -1;
    if (myHitsID < 0) {
        myHitsID = G4SDManager::GetSDMpointer()->GetCollectionID("MyHitsCollection");
    }
    
    // Get the hits collection
    MyHitsCollection* hc = static_cast<MyHitsCollection*>(hce->GetHC(myHitsID));
    
    if (hc) {
        // Process hits
        G4int nHits = hc->entries();
        for (G4int i = 0; i < nHits; i++) {
            MyHit* hit = (*hc)[i];
            
            // Access hit data
            G4double energy = hit->GetEnergy();
            G4ThreeVector position = hit->GetPosition();
            G4String volumeName = hit->GetVolumeName();
            
            // Do something with the hit data...
            G4cout << "Hit in " << volumeName 
                   << " with energy " << energy/keV << " keV"
                   << " at position " << position/mm << " mm" << G4endl;
        }
    }
}
```

## 5. Handling Multiple Instances of Volumes

When you have multiple copies of the same volume type (e.g., an array of PMTs), all copies will use the same sensitive detector. The copy number is used to distinguish between different instances:

```cpp
G4bool MySensitiveDetector::ProcessHits(G4Step* step, G4TouchableHistory*) {
    // Get the copy number
    G4int copyNo = step->GetPreStepPoint()->GetTouchableHandle()->GetCopyNumber();
    
    // Create a new hit
    MyHit* hit = new MyHit();
    hit->SetCopyNumber(copyNo);  // Store the copy number
    
    // Other hit properties...
    
    // Add hit to collection
    fHitsCollection->insert(hit);
    
    return true;
}
```

This allows you to identify which specific instance of a volume was hit, even if they all share the same logical volume and sensitive detector.

## 6. Best Practices

1. **Keep hit classes focused**: Only store the information you actually need
2. **Use appropriate units**: Always apply Geant4 units when setting/getting values
3. **Handle threading properly**: Use G4ThreadLocal for allocators
4. **Document your hits collections**: Add clear comments about what each collection is for
5. **Use meaningful names**: Name your collections based on their purpose or detector type

## 7. Troubleshooting

### No Hits Being Recorded

- Check that volumes are marked as "active" in the geometry editor
- Verify the JSON export includes the `isActive` and `hitsCollectionName` properties
- Ensure the sensitive detector is properly registered with G4SDManager
- Check that the logical volume has the sensitive detector assigned

### Accessing Wrong Hits Collection

- Verify collection names match exactly between JSON and C++ code
- Check the collection ID retrieval using G4SDManager
- Print collection names and IDs for debugging

### Thread-Related Issues

- Ensure proper use of G4ThreadLocal for allocators
- Check thread-local caching of collection IDs
