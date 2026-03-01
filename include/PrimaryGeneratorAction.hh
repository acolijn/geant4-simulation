#ifndef PrimaryGeneratorAction_h
#define PrimaryGeneratorAction_h 1

#include "G4VUserPrimaryGeneratorAction.hh"
#include "globals.hh"

class G4GeneralParticleSource;
class G4Event;

/**
 * @class PrimaryGeneratorAction
 * @brief Handles the generation of primary particles using GPS
 *
 * This class uses the G4GeneralParticleSource (GPS) instead of the
 * simple particle gun.  GPS supports:
 * - Point, volume, and surface sources
 * - Arbitrary energy spectra (mono, linear, power-law, Gaussian, …)
 * - Flexible angular distributions (isotropic, cosine, focused, …)
 * - Confinement to a named physical volume
 * - Multiple overlapping sources with individual intensities
 *
 * All configuration is done at run-time via /gps/ macro commands.
 */
class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction
{
  public:
    /** @brief Constructor – creates the GPS instance */
    PrimaryGeneratorAction();

    /** @brief Destructor */
    ~PrimaryGeneratorAction() override;

    /**
     * @brief Generates primary particles for each event
     * @param anEvent Pointer to the current G4Event
     */
    void GeneratePrimaries(G4Event* anEvent) override;

    /** @brief Accessor for the GPS object */
    const G4GeneralParticleSource* GetGPS() const { return fGPS; }

  private:
    G4GeneralParticleSource* fGPS;  ///< Pointer to the General Particle Source
};

#endif
