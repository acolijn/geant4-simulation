#ifndef RunAction_h
#define RunAction_h 1

#include "G4UserRunAction.hh"
#include "globals.hh"
#include "Rtypes.h"

class TFile;
class TTree;

/**
 * @class RunAction
 * @brief Handles ROOT output creation and management for the simulation
 *
 * This class manages the ROOT file output, creating a TTree structure
 * to store simulation results including:
 * - Energy deposits in liquid xenon
 * - Neutron track lengths
 * - Neutron counting
 */
class RunAction : public G4UserRunAction
{
  public:
    /** 
     * @brief Constructor
     * 
     * Initializes ROOT file and tree pointers to nullptr
     */
    RunAction();

    /** 
     * @brief Destructor
     * 
     * Ensures proper cleanup of ROOT file
     */
    virtual ~RunAction();

    /**
     * @brief Called at the start of each run
     * @param run Pointer to the G4Run (not used)
     *
     * Creates the ROOT file and sets up the TTree structure
     * with all necessary branches for data storage
     */
    virtual void BeginOfRunAction(const G4Run* run);

    /**
     * @brief Called at the end of each run
     * @param run Pointer to the G4Run (not used)
     *
     * Writes the ROOT file and properly closes it
     */
    virtual void EndOfRunAction(const G4Run* run);
    
  private:
    TFile* rootFile;     ///< Pointer to ROOT output file
    TTree* eventTree;    ///< Pointer to main data TTree
    
    // Variables to store in ROOT tree
    Double_t fEnergyDep;    ///< Energy deposit in LXe [MeV]
    Double_t fTrackLength;  ///< Neutron track length [mm]
    Int_t fNeutronCount;    ///< Number of neutrons in event
};

#endif
