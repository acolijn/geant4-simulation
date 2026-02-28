#ifndef MySensitiveDetector_h
#define MySensitiveDetector_h 1

#include "G4VSensitiveDetector.hh"
#include "MyHit.hh"

class G4Step;
class G4HCofThisEvent;

/**
 * @class MySensitiveDetector
 * @brief Default sensitive detector for active volumes
 * 
 * This class processes hits in active volumes and stores them in the MyHitsCollection.
 * It records basic information like energy deposit, position, time, and track ID.
 */
class MySensitiveDetector : public G4VSensitiveDetector {
public:
  MySensitiveDetector(const G4String& name, const G4String& hitsCollectionName = "MyHitsCollection");
  virtual ~MySensitiveDetector();
  
  // Methods from base class
  virtual void Initialize(G4HCofThisEvent* hitCollection);
  virtual G4bool ProcessHits(G4Step* step, G4TouchableHistory* history);
  virtual void EndOfEvent(G4HCofThisEvent* hitCollection);

  /// Set verbosity: 0 = silent, 1 = summary per event, 2 = every hit
  static void   SetVerboseLevel(G4int level) { fVerboseLevel = level; }
  static G4int  GetVerboseLevel()            { return fVerboseLevel; }
  
private:
  MyHitsCollection* fHitsCollection;
  G4int fHitsCollectionID;

  static G4int fVerboseLevel;           ///< Shared print level (default 0)
  static bool  fMessengerCreated;       ///< Ensures messenger is created once

  class HitsMessenger;
  static HitsMessenger* fMessenger;
};

#endif
