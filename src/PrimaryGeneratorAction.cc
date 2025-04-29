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
 * For each event, this method:
 * 1. Generates a random position on the top face of the world volume
 *    - x: Random position within ±15 cm
 *    - y: Random position within ±15 cm
 *    - z: Fixed at top of world volume (-1m)
 * 2. Sets the particle gun position
 * 3. Creates the primary vertex
 *
 * The random distribution ensures good coverage of the liquid xenon target.
 */
void PrimaryGeneratorAction::GeneratePrimaries(G4Event* anEvent)
{
    // Generate random position on top face of world volume
    G4double worldZHalfLength = 1.0*m;
    G4double x0 = 30.0*cm * (G4UniformRand()-0.5);  // Random x in [-15cm, 15cm]
    G4double y0 = 30.0*cm * (G4UniformRand()-0.5);  // Random y in [-15cm, 15cm]
    G4double z0 = -worldZHalfLength;                 // Top of world volume

    // Set position and generate the vertex
    fParticleGun->SetParticlePosition(G4ThreeVector(x0,y0,z0));
    fParticleGun->GeneratePrimaryVertex(anEvent);
}
