#ifndef ActionInitialization_h
#define ActionInitialization_h 1

#include "G4VUserActionInitialization.hh"

/**
 * @class ActionInitialization
 * @brief Initializes all user action classes for the simulation
 *
 * This class is responsible for creating and registering all
 * user action classes, including:
 * - PrimaryGeneratorAction for neutron generation
 * - RunAction for ROOT output management
 *
 * It handles both sequential and multi-threaded execution modes.
 */
class ActionInitialization : public G4VUserActionInitialization
{
  public:
    /** 
     * @brief Constructor
     * 
     * No initialization needed in constructor
     */
    ActionInitialization();

    /** @brief Destructor */
    virtual ~ActionInitialization();

    /**
     * @brief Creates user actions for master thread
     *
     * Called only in multi-threading mode.
     * Creates actions that should only run on the master thread,
     * specifically the RunAction for ROOT file management.
     */
    virtual void BuildForMaster() const;

    /**
     * @brief Creates user actions for worker threads
     *
     * Creates all necessary user action objects:
     * - PrimaryGeneratorAction for neutron generation
     * - RunAction for data collection
     */
    virtual void Build() const;
};

#endif
