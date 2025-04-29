/**
 * @file RunAction.cc
 * @brief Implementation of the RunAction class
 *
 * Handles the creation and management of ROOT output files
 * for storing simulation results.
 */

#include "RunAction.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4UnitsTable.hh"
#include "G4SystemOfUnits.hh"

#include "TFile.h"
#include "TTree.h"

/**
 * @brief Constructor implementation
 *
 * Initializes all member variables:
 * - ROOT file pointer to nullptr
 * - TTree pointer to nullptr
 * - Data variables to zero
 *
 * The actual ROOT objects are created in BeginOfRunAction
 */
RunAction::RunAction()
: G4UserRunAction(),
  rootFile(nullptr),
  eventTree(nullptr),
  fEnergyDep(0.),      // Energy deposit in MeV
  fTrackLength(0.),    // Track length in mm
  fNeutronCount(0)     // Number of neutrons
{ }

/**
 * @brief Destructor implementation
 *
 * Ensures proper cleanup of the ROOT file.
 * The TTree is automatically deleted when the file is deleted.
 */
RunAction::~RunAction()
{
    delete rootFile;
}

/**
 * @brief Initialization at the start of each run
 * @param run Pointer to the G4Run (unused)
 *
 * Creates and configures the ROOT output:
 * 1. Creates a new ROOT file 'neutron_data.root'
 * 2. Creates a TTree 'neutronTree'
 * 3. Sets up branches for:
 *    - Energy deposits (Double_t)
 *    - Track lengths (Double_t)
 *    - Neutron counts (Int_t)
 */
void RunAction::BeginOfRunAction(const G4Run*)
{
    // Create ROOT file and tree
    rootFile = new TFile("neutron_data.root", "RECREATE");
    eventTree = new TTree("neutronTree", "Neutron Transport Data");
    
    // Create branches with appropriate types
    eventTree->Branch("EnergyDeposit", &fEnergyDep, "EnergyDeposit/D");
    eventTree->Branch("TrackLength", &fTrackLength, "TrackLength/D");
    eventTree->Branch("NeutronCount", &fNeutronCount, "NeutronCount/I");
}

/**
 * @brief Cleanup at the end of each run
 * @param run Pointer to the G4Run (unused)
 *
 * Finalizes the ROOT output:
 * 1. Writes all data to disk
 * 2. Properly closes the ROOT file
 *
 * After this method, the ROOT file is ready for analysis.
 */
void RunAction::EndOfRunAction(const G4Run*)
{
    rootFile->Write();
    rootFile->Close();
}
