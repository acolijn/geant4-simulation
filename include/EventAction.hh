#ifndef EventAction_h
#define EventAction_h 1

#include "G4UserEventAction.hh"
#include "globals.hh"

class G4Event;

/**
 * @class EventAction
 * @brief Event action class to process hits collections
 *
 * This class handles the processing of hits collections at the end of each event.
 * It extracts hit information from the MyHitsCollection and can be extended to
 * handle additional custom hits collections.
 */
class EventAction : public G4UserEventAction {
public:
  EventAction();
  virtual ~EventAction();

  virtual void BeginOfEventAction(const G4Event* event);
  virtual void EndOfEventAction(const G4Event* event);

private:
  G4int fMyHitsCollectionID;
  
  // Process the default hits collection
  void ProcessMyHits(const G4Event* event);
};

#endif
