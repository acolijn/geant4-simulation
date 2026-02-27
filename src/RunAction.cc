/**
 * @file RunAction.cc
 * @brief Implementation of the RunAction class
 *
 * Handles the creation and management of ROOT output files.
 * The TTree branches are created dynamically by EventAction.
 */

#include "RunAction.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4UIdirectory.hh"

#include "TFile.h"
#include "TTree.h"

// ---------------------------------------------------------------------------
//  Nested messenger class – mirrors the pattern in DetectorConstruction.cc
// ---------------------------------------------------------------------------
class RunAction::RunActionMessenger : public G4UImessenger
{
  public:
    RunActionMessenger(RunAction* runAction);
    virtual ~RunActionMessenger();
    virtual void SetNewValue(G4UIcommand* command, G4String newValue);

  private:
    RunAction*            fRunAction;
    G4UIdirectory*        fOutputDir;
    G4UIcmdWithAString*   fFileNameCmd;
};

RunAction::RunActionMessenger::RunActionMessenger(RunAction* runAction)
: fRunAction(runAction)
{
    fOutputDir = new G4UIdirectory("/output/");
    fOutputDir->SetGuidance("Output file configuration commands");

    fFileNameCmd = new G4UIcmdWithAString("/output/setFileName", this);
    fFileNameCmd->SetGuidance("Set the ROOT output file name (e.g. myrun.root)");
    fFileNameCmd->SetParameterName("FileName", false);
}

RunAction::RunActionMessenger::~RunActionMessenger()
{
    delete fFileNameCmd;
    delete fOutputDir;
}

void RunAction::RunActionMessenger::SetNewValue(G4UIcommand* command,
                                                 G4String newValue)
{
    if (command == fFileNameCmd) {
        fRunAction->SetOutputFileName(newValue);
    }
}

// ---------------------------------------------------------------------------
//  RunAction implementation
// ---------------------------------------------------------------------------
RunAction::RunAction()
: G4UserRunAction(),
  fMessenger(nullptr),
  fRootFile(nullptr),
  fEventTree(nullptr),
  fOutputFileName("G4sim.root")
{
    fMessenger = new RunActionMessenger(this);
}

RunAction::~RunAction()
{
    delete fMessenger;
    delete fRootFile;
}

void RunAction::BeginOfRunAction(const G4Run*)
{
    // Create ROOT file and tree — branches are added by EventAction
    G4cout << "RunAction: writing output to " << fOutputFileName << G4endl;
    fRootFile = new TFile(fOutputFileName.c_str(), "RECREATE");
    fEventTree = new TTree("events", "Geant4 Simulation Events");
}

void RunAction::EndOfRunAction(const G4Run*)
{
    if (fRootFile) {
        fRootFile->Write();
        fRootFile->Close();
    }
}
