#include "MyHit.hh"

G4ThreadLocal G4Allocator<MyHit>* MyHitAllocator = nullptr;

/**
 * @brief Default constructor
 */
MyHit::MyHit()
: G4VHit(),
  fTrackID(-1),
  fVolumeName(""),
  fPosition(G4ThreeVector()),
  fEnergy(0.),
  fTime(0.)
{}

/**
 * @brief Destructor
 */
MyHit::~MyHit() {}

/**
 * @brief Copy constructor
 */
MyHit::MyHit(const MyHit& right)
: G4VHit() {
  fTrackID = right.fTrackID;
  fVolumeName = right.fVolumeName;
  fPosition = right.fPosition;
  fEnergy = right.fEnergy;
  fTime = right.fTime;
}

/**
 * @brief Assignment operator
 */
const MyHit& MyHit::operator=(const MyHit& right) {
  fTrackID = right.fTrackID;
  fVolumeName = right.fVolumeName;
  fPosition = right.fPosition;
  fEnergy = right.fEnergy;
  fTime = right.fTime;
  return *this;
}

/**
 * @brief Equality operator
 */
G4bool MyHit::operator==(const MyHit& right) const {
  return (this == &right) ? true : false;
}
