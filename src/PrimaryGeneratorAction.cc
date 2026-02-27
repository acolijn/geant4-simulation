/**
 * @file PrimaryGeneratorAction.cc
 * @brief Implementation of the PrimaryGeneratorAction class
 *
 * Handles the generation of primary neutrons for the simulation,
 * including their initial position, energy, and direction.
 */

#include "PrimaryGeneratorAction.hh"

#include "G4LogicalVolumeStore.hh"
#include "G4LogicalVolume.hh"
#include "G4Box.hh"
#include "G4Event.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"

/**
 * @brief Constructor implementation
 *
 * Sets up the particle gun with default neutron properties:
 * - Particle type: neutron
 * - Energy: 1 MeV
 * - Direction: Along positive Z axis (0,0,1)
 * - Number of particles per event: 1
 */
PrimaryGeneratorAction::PrimaryGeneratorAction()
: G4VUserPrimaryGeneratorAction(),
  fParticleGun(nullptr)
{
    G4int n_particle = 1;
    fParticleGun = new G4ParticleGun(n_particle);

    // Configure the particle gun for neutrons
    G4ParticleTable* particleTable = G4ParticleTable::GetParticleTable();
    G4ParticleDefinition* particle = particleTable->FindParticle("neutron");
    fParticleGun->SetParticleDefinition(particle);
    fParticleGun->SetParticleMomentumDirection(G4ThreeVector(0.,0.,1.));
    fParticleGun->SetParticleEnergy(1.0*MeV);  // Initial neutron energy
}

/**
 * @brief Destructor implementation
 *
 * Cleans up the particle gun object to prevent memory leaks.
 */
PrimaryGeneratorAction::~PrimaryGeneratorAction()
{
    delete fParticleGun;
}

/**
 * @brief Generates primary particles for each event
 * @param anEvent The current G4Event being processed
 *
 * Uses the position and direction set by the particle gun
 * (configurable via macro commands /gun/position and /gun/direction).
 */
void PrimaryGeneratorAction::GeneratePrimaries(G4Event* anEvent)
{
    fParticleGun->GeneratePrimaryVertex(anEvent);
}
