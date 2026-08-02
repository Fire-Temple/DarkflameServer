#ifndef MOVERSUBCOMPONENT_H
#define MOVERSUBCOMPONENT_H

#include "RakNetTypes.h"
#include "NiPoint3.h"
#include <string>

#include "dCommonVars.h"
#include "EntityManager.h"
#include "Component.h"
#include "eMovementPlatformState.h"
#include "eReplicaComponentType.h"

/**
 * Sub component for moving platforms that determine the actual current movement state
 */
class MoverSubComponent {
public:
	MoverSubComponent(const NiPoint3& startPos);
	~MoverSubComponent();

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate, bool simpleMover = false, 
	NiPoint3 simpleMoverPos = {}, NiQuaternion simpleMoverRot = {});

	/**
	 * The state the platform is currently in
	 */
	eMovementPlatformState mState = eMovementPlatformState::Stationary;

	/**
	 * The waypoint this platform currently wants to traverse to
	 */
	uint32_t mDesiredWaypointIndex = -1;

	/**
	 * Whether the platform is currently reversing away from the desired waypoint
	 */
	bool mInReverse = false;

	/**
	 * Whether the platform should stop moving when reaching the desired waypoint
	 */
	bool mShouldStopAtDesiredWaypoint = false;

	/**
	 * The percentage of the way between the last point and the desired point
	 */
	float mPercentBetweenPoints = 0;

	/**
	 * The current position of the platform based off current waypoint
	 */
	NiPoint3 mPosition{};
	
	/**
	 * The current rotation of the platform based off current waypoint
	 */
	NiQuaternion mRotation{};
	
	/**
	 * The current velocity of the platform based off current waypoint
	 */
	NiPoint3 mVelocity{};
	
	/**
	 * The current angular velocity of the platform based off current waypoint
	 */
	NiPoint3 mAngularVelocity{};

	/**
	 * The start position of the simple mover platform (waypoint 0)
	 */
	NiPoint3 mSimpleMoverStartPosition{0.0f, 0.0f, 0.0f};

	/**
	 * The end position of the simple mover platform (waypoint 1)
	 */
	NiPoint3 mSimpleMoverEndPosition{0.0f, 0.0f, 0.0f};
	
	/**
	 * The waypoint the platform is (was) at
	 */
	uint32_t mCurrentWaypointIndex;

	/**
	 * The waypoint the platform is attempting to go to
	 */
	uint32_t mNextWaypointIndex;
	
	/**
	 * The last waypoint in the path
	 */
	uint32_t mLastWaypointIndex;	

	/**
	 * The total estimated move time from current waypoint to next waypoint
	 */
	float mTotalMoveTime = 0;

	/**
	 * The time the platform has moved during the moving state
	 */
	float mMoveTimeElapsed = 0;

	/**
	 * The time the platform is stopped before continuing platform movement
	 */
	float mIdleTimeElapsed = 0;

	/**
	 * The speed the platform is currently moving at
	 */
	float mSpeed = 0;

	/**
	 * The time to wait before continuing movement
	 */
	float mWaitTime = 0;
	
	/**
	 * Play once the platform starts
	 */
	std::string mStartGUID;
	
	/**
	 * Play once the platform stops
	 */
	std::string mStopGUID;	
	
	/**
	 * Play during platform pathing
	 */
	std::string mTravelGUID;
};

#endif // MOVERSUBCOMPONENT_H