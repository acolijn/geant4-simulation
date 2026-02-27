#include "EventAction.hh"
#include "MyHit.hh"

#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4HCofThisEvent.hh"
#include "G4SDManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4ios.hh"

/**
 * @brief Constructor
 */
EventAction::EventAction()
: G4UserEventAction(),
  fCollectionsInitialized(false)
{}

/**
 * @brief Destructor
 */
EventAction::~EventAction()
{}

/**
 * @brief Actions at the beginning of each event
 * @param event The current event
 */
void EventAction::BeginOfEventAction(const G4Event* event)
{
  // Print event number for every 100 events
  G4int eventID = event->GetEventID();
  if (eventID % 100 == 0) {
    G4cout << ">>> Event: " << eventID << G4endl;
  }
}

/**
 * @brief Discover all registered hits collection IDs
 */
void EventAction::InitializeCollectionIDs()
{
  G4SDManager* sdManager = G4SDManager::GetSDMpointer();
  G4HCtable* hcTable = sdManager->GetHCtable();
  
  for (G4int i = 0; i < hcTable->entries(); i++) {
    G4String sdName = hcTable->GetSDname(i);
    G4String hcName = hcTable->GetHCname(i);
    G4String fullName = sdName + "/" + hcName;
    G4int id = sdManager->GetCollectionID(fullName);
    fHitsCollectionIDs[hcName] = id;
    G4cout << "Registered hits collection: \"" << hcName 
           << "\" (SD: " << sdName << ", ID: " << id << ")" << G4endl;
  }
  
  fCollectionsInitialized = true;
}

/**
 * @brief Actions at the end of each event
 * @param event The current event
 */
void EventAction::EndOfEventAction(const G4Event* event)
{
  // Discover collections on first event
  if (!fCollectionsInitialized) {
    InitializeCollectionIDs();
  }
  
  // Process all hits collections
  for (const auto& [name, id] : fHitsCollectionIDs) {
    ProcessHitsCollection(event, name, id);
  }
}

/**
 * @brief Process a single hits collection
 * @param event The current event
 * @param name The hits collection name
 * @param id The hits collection ID
 */
void EventAction::ProcessHitsCollection(const G4Event* event, const G4String& name, G4int id)
{
  // Get hits collections
  G4HCofThisEvent* hce = event->GetHCofThisEvent();
  if (!hce) return;
  
  // Get the hits collection
  MyHitsCollection* hitsCollection = 
    static_cast<MyHitsCollection*>(hce->GetHC(id));
  
  if (!hitsCollection) return;
  
  // Process hits
  G4int nHits = hitsCollection->entries();
  
  // Skip processing if no hits
  if (nHits == 0) return;
  
  G4cout << "Event " << event->GetEventID() 
         << " has " << nHits << " hits in \"" << name << "\"" << G4endl;
  
  // Calculate total energy deposit
  G4double totalEdep = 0.0;
  
  // Process each hit
  for (G4int i = 0; i < nHits; i++) {
    MyHit* hit = (*hitsCollection)[i];
    
    // Get hit data
    G4double edep = hit->GetEnergy();
    G4ThreeVector pos = hit->GetPosition();
    G4String volName = hit->GetVolumeName();
    G4double time = hit->GetTime();
    
    // Accumulate energy deposit
    totalEdep += edep;
    
    // Print hit details (can be commented out for production runs)
    G4cout << "  Hit " << i 
           << " in volume " << volName
           << " at position " << pos/mm << " mm"
           << " with energy " << edep/keV << " keV"
           << " at time " << time/ns << " ns"
           << G4endl;
  }
  
  G4cout << "  Total energy deposit in \"" << name << "\": " << totalEdep/keV << " keV" << G4endl;
}
