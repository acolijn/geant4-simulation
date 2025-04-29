#ifndef PrimaryGeneratorAction_h
#define PrimaryGeneratorAction_h 1

#include "G4VUserPrimaryGeneratorAction.hh"
#include "G4ParticleGun.hh"
#include "globals.hh"

class G4ParticleGun;
class G4Event;

/**
 * @class PrimaryGeneratorAction
 * @brief Handles the generation of primary neutrons for the simulation
 *
 * This class configures and manages the particle gun that generates
 * primary neutrons. The neutrons are generated with:
 * - Energy: 1 MeV (configurable through macro)
 * - Position: Random points on top face of world volume
 * - Direction: Downward along Z-axis
 */
class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
  public:
    /** 
     * @brief Constructor
     * 
     * Initializes the particle gun and sets default neutron properties
     */
    PrimaryGeneratorAction();    

    /** @brief Destructor */
    virtual ~PrimaryGeneratorAction();

    /**
     * @brief Generates primary particles for each event
     * @param anEvent Pointer to the current G4Event
     *
     * For each event, this method:
     * 1. Generates a random position on the top face
     * 2. Sets the particle gun position
     * 3. Creates the primary vertex
     */
    virtual void GeneratePrimaries(G4Event* anEvent);         

  private:
    G4ParticleGun* fParticleGun;  ///< Pointer to the particle gun
};

#endif
