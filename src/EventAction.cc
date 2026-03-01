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
#include "G4UIdirectory.hh"
#include "G4UIcmdWithAnInteger.hh"
#include "G4UImessenger.hh"

#include "TTree.h"

// ── Static members ──────────────────────────────────────
G4int EventAction::fSummarize = 0;
bool  EventAction::fSumMessengerCreated = false;
EventAction::SummarizeMessenger* EventAction::fSumMessenger = nullptr;

// ── Nested messenger for /output/setSummarize ───────────
class EventAction::SummarizeMessenger : public G4UImessenger
{
public:
  SummarizeMessenger() {
    // /output/ directory already exists (created by RunAction)
    fCmd = new G4UIcmdWithAnInteger("/output/setSummarize", this);
    fCmd->SetGuidance("0 = per-hit output (default), 1 = summarise per volume");
    fCmd->SetParameterName("flag", false);
    fCmd->SetRange("flag>=0 && flag<=1");
  }
  ~SummarizeMessenger() override { delete fCmd; }

  void SetNewValue(G4UIcommand* cmd, G4String val) override {
    if (cmd == fCmd)
      EventAction::SetSummarize(fCmd->GetNewIntValue(val));
  }
private:
  G4UIcmdWithAnInteger* fCmd;
};

// ----------------------------------------------------------------
EventAction::EventAction()
: G4UserEventAction(),
  fCollectionsInitialized(false),
  fTree(nullptr)
{
  // Create the summarise messenger once
  if (!fSumMessengerCreated) {
    fSumMessenger = new SummarizeMessenger();
    fSumMessengerCreated = true;
  }
}

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
    fNHitsPerVol[det] = {};

    // Create ROOT branches
    fTree->Branch((det + "_nHits").c_str(), &fNHits[det], (det + "_nHits/I").c_str());
    fTree->Branch((det + "_x").c_str(),     &fX[det]);
    fTree->Branch((det + "_y").c_str(),     &fY[det]);
    fTree->Branch((det + "_z").c_str(),     &fZ[det]);
    fTree->Branch((det + "_E").c_str(),     &fE[det]);
    fTree->Branch((det + "_volName").c_str(), &fVolName[det]);
    fTree->Branch((det + "_nHitsPerVol").c_str(), &fNHitsPerVol[det]);

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
    fNHitsPerVol[det].clear();
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

    if (fSummarize == 0) {
      // ── Detailed mode: one entry per hit ──
      fNHits[det] = nHits;

      for (G4int i = 0; i < nHits; i++) {
        MyHit* hit = (*hc)[i];
        G4ThreeVector pos = hit->GetPosition();

        fX[det].push_back(pos.x() / mm);
        fY[det].push_back(pos.y() / mm);
        fZ[det].push_back(pos.z() / mm);
        fE[det].push_back(hit->GetEnergy() / MeV);
        fVolName[det].push_back(std::string(hit->GetVolumeName()));
        fNHitsPerVol[det].push_back(1);
      }
    } else {
      // ── Summary mode: aggregate by volume name ──
      // Use ordered map to keep volume names deterministic
      struct VolAccum {
        double sumE  = 0.0;
        double sumWX = 0.0;
        double sumWY = 0.0;
        double sumWZ = 0.0;
        int    count = 0;
      };
      std::map<std::string, VolAccum> accum;

      for (G4int i = 0; i < nHits; i++) {
        MyHit* hit = (*hc)[i];
        G4ThreeVector pos = hit->GetPosition();
        double e = hit->GetEnergy() / MeV;
        std::string vn = std::string(hit->GetVolumeName());

        auto& a = accum[vn];
        a.sumE  += e;
        a.sumWX += e * (pos.x() / mm);
        a.sumWY += e * (pos.y() / mm);
        a.sumWZ += e * (pos.z() / mm);
        a.count++;
      }

      fNHits[det] = static_cast<Int_t>(accum.size());

      for (const auto& [vn, a] : accum) {
        fE[det].push_back(a.sumE);
        if (a.sumE > 0.0) {
          fX[det].push_back(a.sumWX / a.sumE);
          fY[det].push_back(a.sumWY / a.sumE);
          fZ[det].push_back(a.sumWZ / a.sumE);
        } else {
          fX[det].push_back(0.0);
          fY[det].push_back(0.0);
          fZ[det].push_back(0.0);
        }
        fVolName[det].push_back(vn);
        fNHitsPerVol[det].push_back(a.count);
      }
    }
  }

  // Fill the tree once per event
  fTree->Fill();
}
