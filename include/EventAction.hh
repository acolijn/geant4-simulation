#ifndef EventAction_h
#define EventAction_h 1

#include "G4UserEventAction.hh"
#include "globals.hh"
#include "Rtypes.h"

#include <map>
#include <string>
#include <vector>

class G4Event;
class TTree;

/**
 * @class EventAction
 * @brief Event action class that writes per-detector hit data to ROOT tree
 *
 * This class automatically discovers all hits collections registered by
 * sensitive detectors and creates ROOT tree branches for each detector.
 * For each detector, the following branches are created:
 *   - <det>_nHits  (Int_t)                number of hits
 *   - <det>_x      (vector<double>)       hit x positions [mm]
 *   - <det>_y      (vector<double>)       hit y positions [mm]
 *   - <det>_z      (vector<double>)       hit z positions [mm]
 *   - <det>_E      (vector<double>)       energy deposits  [MeV]
 */
class EventAction : public G4UserEventAction {
public:
  EventAction();
  virtual ~EventAction();

  virtual void BeginOfEventAction(const G4Event* event);
  virtual void EndOfEventAction(const G4Event* event);

private:
  /// Discover all registered hits collections and create ROOT branches
  void InitializeCollections();

  /// Cache of hits collection IDs by name
  std::map<G4String, G4int> fHitsCollectionIDs;
  bool fCollectionsInitialized;

  /// Pointer to the TTree owned by RunAction
  TTree* fTree;

  // ---- Per-detector ROOT branch data ----
  /// Number of hits per detector per event
  std::map<std::string, Int_t>                 fNHits;
  /// Hit x-coordinates per detector
  std::map<std::string, std::vector<double>>   fX;
  /// Hit y-coordinates per detector
  std::map<std::string, std::vector<double>>   fY;
  /// Hit z-coordinates per detector
  std::map<std::string, std::vector<double>>   fZ;
  /// Energy deposit per hit per detector
  std::map<std::string, std::vector<double>>   fE;
  /// Physical volume name per hit per detector
  std::map<std::string, std::vector<std::string>> fVolName;
};

#endif
