#include "EventAction.hh"
#include "RunAction.hh"
#include "MyHit.hh"

#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4HCofThisEvent.hh"
#include "G4RunManager.hh"
#include "G4SDManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4ios.hh"

#include "TTree.h"

// ----------------------------------------------------------------
EventAction::EventAction()
: G4UserEventAction(),
  fCollectionsInitialized(false),
  fTree(nullptr)
{}

EventAction::~EventAction()
{}

// ----------------------------------------------------------------
// Called once on the first event: discover every hits collection
// that was registered by the sensitive detectors and create a
// matching set of branches in the ROOT TTree.
// ----------------------------------------------------------------
void EventAction::InitializeCollections()
{
  // Get the TTree from RunAction
  auto runAction = static_cast<const RunAction*>(
      G4RunManager::GetRunManager()->GetUserRunAction());
  fTree = runAction->GetEventTree();

  // Discover all hits collections
  G4SDManager* sdManager = G4SDManager::GetSDMpointer();
  G4HCtable*   hcTable   = sdManager->GetHCtable();

  for (G4int i = 0; i < hcTable->entries(); i++) {
    G4String sdName   = hcTable->GetSDname(i);
    G4String hcName   = hcTable->GetHCname(i);
    G4String fullName = sdName + "/" + hcName;
    G4int    id       = sdManager->GetCollectionID(fullName);

    // Use the hits-collection name as the detector key
    std::string det = std::string(hcName);

    fHitsCollectionIDs[hcName] = id;

    // Initialise data maps for this detector
    fNHits[det] = 0;
    fX[det]     = {};
    fY[det]     = {};
    fZ[det]     = {};
    fE[det]     = {};
    fVolName[det] = {};

    // Create ROOT branches
    fTree->Branch((det + "_nHits").c_str(), &fNHits[det], (det + "_nHits/I").c_str());
    fTree->Branch((det + "_x").c_str(),     &fX[det]);
    fTree->Branch((det + "_y").c_str(),     &fY[det]);
    fTree->Branch((det + "_z").c_str(),     &fZ[det]);
    fTree->Branch((det + "_E").c_str(),     &fE[det]);
    fTree->Branch((det + "_volName").c_str(), &fVolName[det]);

    G4cout << "Created ROOT branches for detector \"" << det
           << "\" (SD: " << sdName << ", ID: " << id << ")" << G4endl;
  }

  fCollectionsInitialized = true;
}

// ----------------------------------------------------------------
void EventAction::BeginOfEventAction(const G4Event* event)
{
  G4int eventID = event->GetEventID();
  if (eventID % 1000 == 0) {
    G4cout << ">>> Event: " << eventID << G4endl;
  }
}

// ----------------------------------------------------------------
void EventAction::EndOfEventAction(const G4Event* event)
{
  // Lazy initialisation of branch bookkeeping
  if (!fCollectionsInitialized) {
    InitializeCollections();
  }

  // Clear all vectors before filling
  for (auto& [det, _] : fNHits) {
    fNHits[det] = 0;
    fX[det].clear();
    fY[det].clear();
    fZ[det].clear();
    fE[det].clear();
    fVolName[det].clear();
  }

  G4HCofThisEvent* hce = event->GetHCofThisEvent();
  if (!hce) {
    fTree->Fill();
    return;
  }

  // Loop over every registered detector and fill vectors
  for (const auto& [hcName, id] : fHitsCollectionIDs) {
    auto* hc = static_cast<MyHitsCollection*>(hce->GetHC(id));
    if (!hc) continue;

    std::string det  = std::string(hcName);
    G4int       nHits = hc->entries();
    fNHits[det] = nHits;

    for (G4int i = 0; i < nHits; i++) {
      MyHit* hit = (*hc)[i];
      G4ThreeVector pos = hit->GetPosition();

      fX[det].push_back(pos.x() / mm);
      fY[det].push_back(pos.y() / mm);
      fZ[det].push_back(pos.z() / mm);
      fE[det].push_back(hit->GetEnergy() / MeV);
      fVolName[det].push_back(std::string(hit->GetVolumeName()));
    }
  }

  // Fill the tree once per event
  fTree->Fill();
}
