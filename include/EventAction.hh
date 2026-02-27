#ifndef EventAction_h
#define EventAction_h 1

#include "G4UserEventAction.hh"
#include "globals.hh"

#include <map>
#include <string>

class G4Event;

/**
 * @class EventAction
 * @brief Event action class to process hits collections
 *
 * This class handles the processing of hits collections at the end of each event.
 * It automatically discovers and processes all hits collections registered by
 * the sensitive detectors defined in the geometry JSON configuration.
 */
class EventAction : public G4UserEventAction {
public:
  EventAction();
  virtual ~EventAction();

  virtual void BeginOfEventAction(const G4Event* event);
  virtual void EndOfEventAction(const G4Event* event);

private:
  /// Cache of hits collection IDs by name
  std::map<G4String, G4int> fHitsCollectionIDs;
  bool fCollectionsInitialized;
  
  /// Discover all registered hits collections
  void InitializeCollectionIDs();
  
  /// Process a single hits collection by ID
  void ProcessHitsCollection(const G4Event* event, const G4String& name, G4int id);
};

#endif
