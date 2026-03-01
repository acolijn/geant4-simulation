#ifndef EventAction_h
#define EventAction_h 1

#include "G4UserEventAction.hh"
#include "G4UImessenger.hh"
#include "globals.hh"
#include "Rtypes.h"

#include <map>
#include <string>
#include <vector>

class G4Event;
class TTree;
class G4UIcmdWithAnInteger;
class G4UIdirectory;

/**
 * @class EventAction
 * @brief Event action class that writes per-detector hit data to ROOT tree
 *
 * This class automatically discovers all hits collections registered by
 * sensitive detectors and creates ROOT tree branches for each detector.
 *
 * Two output modes are available (controlled via /output/setSummarize):
 *   - **Detailed** (default, 0): one entry per hit
 *     - <det>_nHits, <det>_x/y/z, <det>_E, <det>_volName
 *   - **Summarised** (1): one entry per unique volume per event
 *     - <det>_nHits  = number of volumes hit
 *     - <det>_E      = summed energy deposit per volume  [MeV]
 *     - <det>_x/y/z  = energy-weighted average position  [mm]
 *     - <det>_volName = unique volume name
 *     - <det>_nHitsPerVol = number of raw hits merged into each summary
 */
class EventAction : public G4UserEventAction {
public:
  EventAction();
  virtual ~EventAction();

  virtual void BeginOfEventAction(const G4Event* event);
  virtual void EndOfEventAction(const G4Event* event);

  /// Set summarisation mode: 0 = detailed (per-hit), 1 = per-volume summary
  static void  SetSummarize(G4int val) { fSummarize = val; }
  static G4int GetSummarize()          { return fSummarize; }

private:
  /// Discover all registered hits collections and create ROOT branches
  void InitializeCollections();

  /// Cache of hits collection IDs by name
  std::map<G4String, G4int> fHitsCollectionIDs;
  bool fCollectionsInitialized;

  /// Pointer to the TTree owned by RunAction
  TTree* fTree;

  // ---- Per-detector ROOT branch data ----
  std::map<std::string, Int_t>                    fNHits;
  std::map<std::string, std::vector<double>>      fX;
  std::map<std::string, std::vector<double>>      fY;
  std::map<std::string, std::vector<double>>      fZ;
  std::map<std::string, std::vector<double>>      fE;
  std::map<std::string, std::vector<std::string>> fVolName;
  std::map<std::string, std::vector<int>>         fNHitsPerVol;

  // ---- Summarisation flag & messenger ----
  static G4int fSummarize;

  class SummarizeMessenger;
  static SummarizeMessenger* fSumMessenger;
  static bool fSumMessengerCreated;
};

#endif
