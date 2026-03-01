/**
 * @file PrimaryGeneratorAction.cc
 * @brief Implementation of the PrimaryGeneratorAction class
 *
 * Uses the G4GeneralParticleSource (GPS) for maximum flexibility.
 * All source parameters (particle, energy, position, angular distribution)
 * are configured via /gps/ macro commands at run-time.
 */

#include "PrimaryGeneratorAction.hh"

#include "G4GeneralParticleSource.hh"
#include "G4Event.hh"
#include "G4SystemOfUnits.hh"

/**
 * @brief Constructor – creates the GPS instance
 *
 * GPS defaults to a 1 MeV geantino at the origin with isotropic
 * angular distribution.  Override everything through /gps/ macros.
 */
PrimaryGeneratorAction::PrimaryGeneratorAction()
: G4VUserPrimaryGeneratorAction(),
  fGPS(new G4GeneralParticleSource())
{
    // No hard-coded defaults – GPS is fully configured via macro commands.
}

/**
 * @brief Destructor
 */
PrimaryGeneratorAction::~PrimaryGeneratorAction()
{
    delete fGPS;
}

/**
 * @brief Generates primary particles for each event
 * @param anEvent The current G4Event being processed
 */
void PrimaryGeneratorAction::GeneratePrimaries(G4Event* anEvent)
{
    fGPS->GeneratePrimaryVertex(anEvent);
}
