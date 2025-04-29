/**
 * @file ActionInitialization.cc
 * @brief Implementation of the ActionInitialization class
 */

#include "ActionInitialization.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"

/**
 * @brief Constructor implementation
 * 
 * Calls the base class constructor. No additional initialization needed.
 */
ActionInitialization::ActionInitialization()
 : G4VUserActionInitialization()
{}

/**
 * @brief Destructor implementation
 * 
 * No cleanup needed as user actions are managed by Geant4 kernel.
 */
ActionInitialization::~ActionInitialization()
{}

/**
 * @brief Implementation of master thread initialization
 * 
 * Creates only the RunAction for the master thread in MT mode.
 * This ensures proper handling of the ROOT output file in
 * multi-threaded execution.
 */
void ActionInitialization::BuildForMaster() const
{
    SetUserAction(new RunAction);
}

/**
 * @brief Implementation of worker thread initialization
 * 
 * Creates all necessary user actions for simulation:
 * 1. PrimaryGeneratorAction - Creates neutrons
 * 2. RunAction - Handles data collection
 *
 * This method is called for each worker thread in MT mode,
 * and for the main thread in sequential mode.
 */
void ActionInitialization::Build() const
{
    SetUserAction(new PrimaryGeneratorAction);
    SetUserAction(new RunAction);
}
