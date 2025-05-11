#ifndef MyHit_h
#define MyHit_h 1

#include "G4VHit.hh"
#include "G4THitsCollection.hh"
#include "G4Allocator.hh"
#include "G4ThreeVector.hh"
#include "G4Threading.hh"

/**
 * @class MyHit
 * @brief Default hit class for storing information about energy deposits in active volumes
 * 
 * This class stores basic information about hits in active volumes, including:
 * - Track ID
 * - Volume name (to identify which detector was hit)
 * - Position
 * - Energy deposit
 * - Time
 */
class MyHit : public G4VHit {
public:
  MyHit();
  virtual ~MyHit();
  MyHit(const MyHit&);
  
  // Operators
  const MyHit& operator=(const MyHit&);
  G4bool operator==(const MyHit&) const;
  
  inline void* operator new(size_t);
  inline void operator delete(void*);
  
  // Setters
  void SetTrackID(G4int track) { fTrackID = track; }
  void SetVolumeName(const G4String& name) { fVolumeName = name; }
  void SetPosition(G4ThreeVector xyz) { fPosition = xyz; }
  void SetEnergy(G4double e) { fEnergy = e; }
  void SetTime(G4double t) { fTime = t; }
  
  // Getters
  G4int GetTrackID() const { return fTrackID; }
  G4String GetVolumeName() const { return fVolumeName; }
  G4ThreeVector GetPosition() const { return fPosition; }
  G4double GetEnergy() const { return fEnergy; }
  G4double GetTime() const { return fTime; }
  
private:
  G4int fTrackID;
  G4String fVolumeName;
  G4ThreeVector fPosition;
  G4double fEnergy;
  G4double fTime;
};

// Define the hits collection type
typedef G4THitsCollection<MyHit> MyHitsCollection;

// Memory allocation
extern G4ThreadLocal G4Allocator<MyHit>* MyHitAllocator;

inline void* MyHit::operator new(size_t) {
  if (!MyHitAllocator) MyHitAllocator = new G4Allocator<MyHit>;
  return (void*)MyHitAllocator->MallocSingle();
}

inline void MyHit::operator delete(void* hit) {
  MyHitAllocator->FreeSingle((MyHit*)hit);
}

#endif
