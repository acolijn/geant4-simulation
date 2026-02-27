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

#include "TFile.h"
#include "TTree.h"

RunAction::RunAction()
: G4UserRunAction(),
  fRootFile(nullptr),
  fEventTree(nullptr)
{}

RunAction::~RunAction()
{
    delete fRootFile;
}

void RunAction::BeginOfRunAction(const G4Run*)
{
    // Create ROOT file and tree — branches are added by EventAction
    fRootFile = new TFile("GeoTest.root", "RECREATE");
    fEventTree = new TTree("events", "Geant4 Simulation Events");
}

void RunAction::EndOfRunAction(const G4Run*)
{
    if (fRootFile) {
        fRootFile->Write();
        fRootFile->Close();
    }
}
