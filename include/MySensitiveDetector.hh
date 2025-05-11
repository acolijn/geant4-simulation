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
  MySensitiveDetector(const G4String& name);
  virtual ~MySensitiveDetector();
  
  // Methods from base class
  virtual void Initialize(G4HCofThisEvent* hitCollection);
  virtual G4bool ProcessHits(G4Step* step, G4TouchableHistory* history);
  virtual void EndOfEvent(G4HCofThisEvent* hitCollection);
  
private:
  MyHitsCollection* fHitsCollection;
  G4int fHitsCollectionID;
};

#endif
