#ifndef RunAction_h
#define RunAction_h 1

#include "G4UserRunAction.hh"
#include "G4UImessenger.hh"
#include "G4UIcmdWithAString.hh"
#include "globals.hh"

class TFile;
class TTree;

/**
 * @class RunAction
 * @brief Handles ROOT output creation and management for the simulation
 *
 * This class manages the ROOT file output, creating a TTree structure.
 * Branches are created dynamically by EventAction for each sensitive detector.
 */
class RunAction : public G4UserRunAction
{
  public:
    RunAction();
    virtual ~RunAction();

    virtual void BeginOfRunAction(const G4Run* run);
    virtual void EndOfRunAction(const G4Run* run);

    /// Get the event TTree (used by EventAction to create branches and fill)
    TTree* GetEventTree() const { return fEventTree; }

    /// Set the ROOT output file name (called from macro)
    void SetOutputFileName(const G4String& name) { fOutputFileName = name; }

    /// Get the current output file name
    G4String GetOutputFileName() const { return fOutputFileName; }
    
  private:
    class RunActionMessenger;
    RunActionMessenger* fMessenger;       ///< Messenger for UI commands

    TFile* fRootFile;     ///< Pointer to ROOT output file
    TTree* fEventTree;    ///< Pointer to main data TTree
    G4String fOutputFileName;  ///< Configurable output file name
};

#endif
