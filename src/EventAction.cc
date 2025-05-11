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
  fMyHitsCollectionID(-1)
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
 * @brief Actions at the end of each event
 * @param event The current event
 */
void EventAction::EndOfEventAction(const G4Event* event)
{
  // Process hits from the default hits collection
  ProcessMyHits(event);
}

/**
 * @brief Process hits from the default MyHitsCollection
 * @param event The current event
 */
void EventAction::ProcessMyHits(const G4Event* event)
{
  // Get hits collections
  G4HCofThisEvent* hce = event->GetHCofThisEvent();
  if (!hce) return;
  
  // Get the collection ID (do this once and cache it)
  if (fMyHitsCollectionID < 0) {
    fMyHitsCollectionID = G4SDManager::GetSDMpointer()->GetCollectionID("MyHitsCollection");
  }
  
  // Get the hits collection
  MyHitsCollection* hitsCollection = 
    static_cast<MyHitsCollection*>(hce->GetHC(fMyHitsCollectionID));
  
  if (!hitsCollection) return;
  
  // Process hits
  G4int nHits = hitsCollection->entries();
  
  // Skip processing if no hits
  if (nHits == 0) return;
  
  G4cout << "Event " << event->GetEventID() 
         << " has " << nHits << " hits in MyHitsCollection" << G4endl;
  
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
  
  G4cout << "  Total energy deposit: " << totalEdep/keV << " keV" << G4endl;
  
  // Here you could add code to:
  // - Write hits to an output file
  // - Fill histograms
  // - Perform analysis
}
