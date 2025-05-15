#include "MySensitiveDetector.hh"
#include "G4HCofThisEvent.hh"
#include "G4Step.hh"
#include "G4ThreeVector.hh"
#include "G4SDManager.hh"
#include "G4ios.hh"
#include "G4SystemOfUnits.hh"

/**
 * @brief Constructor
 * @param name Name of the sensitive detector
 */
MySensitiveDetector::MySensitiveDetector(const G4String& name)
: G4VSensitiveDetector(name),
  fHitsCollection(nullptr),
  fHitsCollectionID(-1)
{
  collectionName.insert("MyHitsCollection");
}

/**
 * @brief Destructor
 */
MySensitiveDetector::~MySensitiveDetector() {}

/**
 * @brief Initialize at the beginning of each event
 * @param hce Hits collection of this event
 */
void MySensitiveDetector::Initialize(G4HCofThisEvent* hce)
{
  // Create hits collection
  G4cout << "Initializing hits collection for " << SensitiveDetectorName << G4endl;
  fHitsCollection = new MyHitsCollection(SensitiveDetectorName, collectionName[0]);
  
  // Add this collection to the HCE
  if (fHitsCollectionID < 0) {
    fHitsCollectionID = G4SDManager::GetSDMpointer()->GetCollectionID(collectionName[0]);
  }
  hce->AddHitsCollection(fHitsCollectionID, fHitsCollection);
}

/**
 * @brief Process hits during tracking
 * @param step Current step
 * @param history Touchable history (not used)
 * @return True if the hit was processed
 */
G4bool MySensitiveDetector::ProcessHits(G4Step* step, G4TouchableHistory*)
{
  // Energy deposit
  G4double edep = step->GetTotalEnergyDeposit();
  
  // Only record hits with energy deposit
  if (edep == 0.) return false;
  
  // Create a new hit
  MyHit* hit = new MyHit();
  
  // Set hit properties
  hit->SetTrackID(step->GetTrack()->GetTrackID());
  hit->SetVolumeName(step->GetPreStepPoint()->GetPhysicalVolume()->GetName());
  hit->SetPosition(step->GetPostStepPoint()->GetPosition());
  hit->SetEnergy(edep);
  hit->SetTime(step->GetPostStepPoint()->GetGlobalTime());
  
  // Add hit to collection
  fHitsCollection->insert(hit);
  
  return true;
}

/**
 * @brief End of event processing
 * @param hce Hits collection of this event
 */
void MySensitiveDetector::EndOfEvent(G4HCofThisEvent*)
{
  // Print hits collection summary
  G4int nHits = fHitsCollection->entries();
  
  G4cout << "MyHitsCollection has " << nHits << " hits." << G4endl;
  
  // Optional: Print details of each hit
  for (G4int i = 0; i < nHits; i++) {
    MyHit* hit = (*fHitsCollection)[i];
    G4cout << "  Hit " << i 
           << " in volume " << hit->GetVolumeName()
           << " at position " << hit->GetPosition()/mm << " mm"
           << " with energy " << hit->GetEnergy()/keV << " keV"
           << " at time " << hit->GetTime()/ns << " ns"
           << G4endl;
  }
}
