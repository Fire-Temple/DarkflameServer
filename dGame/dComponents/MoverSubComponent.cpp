#include "MoverSubComponent.h"
#include "BitStream.h"
#include "GeneralUtils.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "Logger.h"
#include "GameMessages.h"
#include "CppScripts.h"

#include "PhantomPhysicsComponent.h"
#include "ProximityMonitorComponent.h"
#include "dpEntity.h"

#include "Logger.h"

#include <limits>
#include <glm/gtc/quaternion.hpp>

MoverSubComponent::MoverSubComponent(const NiPoint3& startPos) {
	
	mPosition = startPos;
	
	mState = eMovementPlatformState::Stopped;
	mDesiredWaypointIndex = -1;
	mShouldStopAtDesiredWaypoint = false;

	mPercentBetweenPoints = 0.0f;
	
	mInReverse = false;
	mCurrentWaypointIndex = 0;
	mNextWaypointIndex = 1; // mCurrentWaypointIndex + 1;
	
	mLastWaypointIndex = 1;

	mTotalMoveTime = 0.0f;

	mMoveTimeElapsed = 0.0f;
	mIdleTimeElapsed = 0.0f;
}

MoverSubComponent::~MoverSubComponent() = default;

void MoverSubComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate, bool simpleMover, NiPoint3 simpleMoverPos, NiQuaternion simpleMoverRot) {
	outBitStream.Write<bool>(true); // has mover info

	if (simpleMover) {
	// -- LOCATION INFO --
		outBitStream.Write<bool>(true); // has location info
	
		// position
		outBitStream.Write<float_t>(simpleMoverPos.x);
		outBitStream.Write<float_t>(simpleMoverPos.y);
		outBitStream.Write<float_t>(simpleMoverPos.z);

		// rotation
		outBitStream.Write<float_t>(simpleMoverRot.w);
		outBitStream.Write<float_t>(simpleMoverRot.x);
		outBitStream.Write<float_t>(simpleMoverRot.y);
		outBitStream.Write<float_t>(simpleMoverRot.z);
		
	// -- EXTRA INFO --	
		outBitStream.Write<bool>(true); // has extra info

		// movement state
		outBitStream.Write(mState);

		// starting waypoint
		outBitStream.Write<uint32_t>(mCurrentWaypointIndex);

		// isInReverse
		outBitStream.Write<bool>(mInReverse);
		
//		LOG_DEBUG(
//			"simpleMover data serialized: hasExtraInfo=true state=%d currentWaypoint=%u inReverse=%s",
//			mState, mCurrentWaypointIndex, mInReverse ? "true" : "false");

	} else {	

		outBitStream.Write(mState);
		outBitStream.Write<int32_t>(mDesiredWaypointIndex);
		outBitStream.Write(mShouldStopAtDesiredWaypoint);
		outBitStream.Write(mInReverse);

		outBitStream.Write<float_t>(mPercentBetweenPoints);

		outBitStream.Write<float_t>(mPosition.x);
		outBitStream.Write<float_t>(mPosition.y);
		outBitStream.Write<float_t>(mPosition.z);

		outBitStream.Write<uint32_t>(mCurrentWaypointIndex);
		outBitStream.Write<uint32_t>(mNextWaypointIndex);

		outBitStream.Write<float_t>(mIdleTimeElapsed); // Idle time elapsed
		outBitStream.Write<float_t>(mMoveTimeElapsed); // Move time elapsed
	}
}