#include "MovingPlatformComponent.h"
#include "BitStream.h"
#include "GeneralUtils.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "Logger.h"
#include "GameMessages.h"
#include "CppScripts.h"
#include "SimplePhysicsComponent.h"
#include "Zone.h"

#include "Logger.h"

#include <chrono>
#include <limits>

MoverSubComponent::MoverSubComponent(const NiPoint3& startPos) {
	
	mPosition = startPos;

	mLegStartTime = {};
	mLegTotalDistance = 0.0f;
	mLegDistanceProgress = 0.0f;
	
	mState = eMovementPlatformState::Stopped;
	mDesiredWaypointIndex = -1;
	mShouldStopAtDesiredWaypoint = false;

	mPercentBetweenPoints = 0.0f;
	
	mInReverse = false;
	mCurrentWaypointIndex = 0;
	mNextWaypointIndex = 1; // mCurrentWaypointIndex + 1;
	
	mLastWaypointIndex = 1;

	mIdleTimeElapsed = 0.0f;
	
	mScheduledWaypoint = -1;
	

}

MoverSubComponent::~MoverSubComponent() = default;

void MoverSubComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate, bool simpleMover, NiPoint3 simpleMoverPos, NiQuaternion simpleMoverRot) {
	outBitStream.Write<bool>(true);

	if (simpleMover) {		
		outBitStream.Write<float_t>(simpleMoverPos.x);
		outBitStream.Write<float_t>(simpleMoverPos.y);
		outBitStream.Write<float_t>(simpleMoverPos.z);

		outBitStream.Write<float_t>(simpleMoverRot.w);
		outBitStream.Write<float_t>(simpleMoverRot.x);
		outBitStream.Write<float_t>(simpleMoverRot.y);
		outBitStream.Write<float_t>(simpleMoverRot.z);		

		outBitStream.Write(mState);

		// starting waypoint
		outBitStream.Write<uint32_t>(0);

		// isInReverse
		outBitStream.Write<bool>(false);
		
		LOG_DEBUG("simpleMover data serialized");

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

		outBitStream.Write<float_t>(mIdleTimeElapsed);
		outBitStream.Write<float_t>(0.0f); // Move time elapsed
	}
}

//------------- MovingPlatformComponent below --------------

MovingPlatformComponent::MovingPlatformComponent(Entity* parent, const int32_t componentID, const std::string& pathName) : Component(parent, componentID) {
	// do we have a simple mover?
	if (m_Parent->GetVar<bool>(u"platformIsSimpleMover"))
		m_MoverSubComponentType = eMoverSubComponentType::simpleMover;
	else 
		m_MoverSubComponentType = eMoverSubComponentType::mover;
	
	m_MoverSubComponent = new MoverSubComponent(m_Parent->GetDefaultPosition());
	m_PathName = GeneralUtils::ASCIIToUTF16(pathName);
	m_Path = Game::zoneManager->GetZone()->GetPath(pathName);
	
	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);
	
	m_TimeBased = false;
	if (m_Path && m_Path->pathType == PathType::MovingPlatform)
		m_TimeBased = static_cast<bool>(m_Path->movingPlatform.timeBasedMovement);


	// sounds
	subComponent->mStartGUID = GeneralUtils::UTF16ToWTF8(m_Parent->GetVar<std::u16string>(u"platformSoundStart"));
	subComponent->mStopGUID = GeneralUtils::UTF16ToWTF8(m_Parent->GetVar<std::u16string>(u"platformSoundStop"));
	subComponent->mTravelGUID = GeneralUtils::UTF16ToWTF8(m_Parent->GetVar<std::u16string>(u"platformSoundTravel"));

	if (m_Path != nullptr) {
		subComponent->mLastWaypointIndex = GetLastWaypointIndex();
		if (m_Parent->GetVar<bool>(u"platformStartAtEnd")) {
			// get last waypoint
			const auto& lastWaypoint = m_Path->pathWaypoints[GetLastWaypointIndex()];
			
			subComponent->mInReverse = true;
			
			subComponent->mPosition = lastWaypoint.position;
			subComponent->mRotation = lastWaypoint.rotation;
			subComponent->mCurrentWaypointIndex = GetLastWaypointIndex();
			subComponent->mNextWaypointIndex = GetLastWaypointIndex() - 1;
			
			// StartPathing handles this
//			if (m_Path->pathBehavior == PathBehavior::Once) subComponent->mDesiredWaypointIndex = 0;	


		} else if (m_Parent->HasVar(u"attached_path_start")) {
			PathBehavior behavior = static_cast<PathBehavior>(m_Path->pathBehavior);
			auto start = m_Parent->GetVar<uint32_t>(u"attached_path_start");
			if (start >= 1 && start <= GetLastWaypointIndex()) {
				subComponent->mCurrentWaypointIndex = start;
				subComponent->mPosition = m_Path->pathWaypoints[start].position;
				if (start == GetLastWaypointIndex()) {
					if (behavior != PathBehavior::Loop) {
						subComponent->mInReverse = true;
						subComponent->mNextWaypointIndex = start - 1;
					} else {
						subComponent->mNextWaypointIndex = 0;				
					}
					
				} else {
					subComponent->mNextWaypointIndex = start + 1;
				}
			}
		}
		auto arriveSound = static_cast<std::string>(m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex].movingPlatform.arriveSound);
		auto travelSound = static_cast<std::string>(m_Path->movingPlatform.platformTravelSound);
		auto departSound = static_cast<std::string>(m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex].movingPlatform.departSound);		
	
		if (!arriveSound.empty())
			subComponent->mStopGUID = arriveSound;
		if (!travelSound.empty()) 
			subComponent->mTravelGUID = travelSound;
		if (!departSound.empty())
			subComponent->mStartGUID = departSound;
		
	} else {
		LOG_DEBUG("Path not found: %s", pathName.c_str());
		LOG_DEBUG("using m_Parent position and rotation");
		subComponent->mPosition = m_Parent->GetPosition();
		subComponent->mRotation = m_Parent->GetRotation();
	}
	
	m_NoAutoStart = !m_Parent->GetVar<bool>(u"startPathingOnLoad");
	// if we serlialize later, clients should not start pathing without server
	//	m_NoAutoStart = true;
	
}

MovingPlatformComponent::~MovingPlatformComponent() {
	delete static_cast<MoverSubComponent*>(m_MoverSubComponent);
}

void MovingPlatformComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	// Still no need to serialize

	if (!m_Serialize) {
		outBitStream.Write<bool>(false);
		outBitStream.Write<bool>(false);

		return;
	}

	outBitStream.Write<bool>(true);

	auto hasPath = m_HasPath && !m_PathName.empty();
	outBitStream.Write(hasPath);

	if (hasPath) {
		// Is on rail
		outBitStream.Write1();

		outBitStream.Write<uint16_t>(m_PathName.size());
		for (const auto& c : m_PathName) {
			outBitStream.Write<uint16_t>(c);
		}

		// Starting point
		outBitStream.Write<uint32_t>(0);

		// Reverse
		outBitStream.Write<bool>(false);
	}

	const auto hasPlatform = m_MoverSubComponent != nullptr;
	outBitStream.Write<bool>(hasPlatform);

	if (hasPlatform) {
		auto* mover = static_cast<MoverSubComponent*>(m_MoverSubComponent);
		outBitStream.Write(m_MoverSubComponentType);

		if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
			// TODO
			LOG_DEBUG("attempt serialize a simpleMover");
			mover->Serialize(outBitStream, bIsInitialUpdate);
			mover->Serialize(outBitStream, bIsInitialUpdate, true, m_Parent->GetPosition(), m_Parent->GetRotation());
		} else {
			mover->Serialize(outBitStream, bIsInitialUpdate);
		}
	}
}

void MovingPlatformComponent::OnQuickBuildInitilized() {
	StopPathing();
}

void MovingPlatformComponent::OnCompleteQuickBuild() {
	if (m_NoAutoStart)
		return;

	StartPathing();
}

void MovingPlatformComponent::GotoWaypoint(uint32_t index, bool stopAtWaypoint, bool immediate) {
    auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	int32_t originalCurrentWaypoint = subComponent->mCurrentWaypointIndex;
	float currentPercent = subComponent->mPercentBetweenPoints;
	uint32_t lastIndex = GetLastWaypointIndex();

	// do we have a simple mover?
	if (m_Parent->GetVar<bool>(u"platformIsSimpleMover")) {		
		LOG_DEBUG("GotoWaypoint called on a simple mover");	
		SimpleMove();
		return;
	}

	if (subComponent->mShouldStopAtDesiredWaypoint != stopAtWaypoint) {
		subComponent->mShouldStopAtDesiredWaypoint = stopAtWaypoint;
	}

	if (m_Path == nullptr) {
		// TODO
        return;
	}	

    if (subComponent->mDesiredWaypointIndex == index) {
		if (m_PathingStopped) {
			// we just need a start
			StartPathing(true);
		}
		return;	
	}
	
	subComponent->mDesiredWaypointIndex = index;

	if (subComponent->mPercentBetweenPoints >= 0.97f) {
		LOG_DEBUG("WARNING GotoWaypoint was called with very high percent, did something break?");
		if (!m_PathingStopped) subComponent->mPercentBetweenPoints = 0.98f;
	}	
	
	// finish leg, then path to waypoint	
	else if (!immediate) {		
	
		if (m_PathingStopped) 
			StartPathing(true);

		return;
	}		

	if ((index <= subComponent->mCurrentWaypointIndex && !subComponent->mInReverse) || 
		(index >= subComponent->mCurrentWaypointIndex && subComponent->mInReverse)) {		
		// we need to flip the direction	
	
	
		// update percent for checks
		float distance = 0.0f;
		if (subComponent->mLegTotalDistance > 0.0f) distance = CalculateDistance();
		
		subComponent->mLegDistanceProgress += distance;
		subComponent->mLegDistanceProgress = std::clamp(subComponent->mLegDistanceProgress, 0.0f, subComponent->mLegTotalDistance);
		
		
		// looping path behavior isn't great for this scenario, temporary fix
		PathBehavior behavior = static_cast<PathBehavior>(m_Path->pathBehavior);
		if (behavior != PathBehavior::Loop) {
			subComponent->mInReverse = !subComponent->mInReverse;
			std::swap(subComponent->mCurrentWaypointIndex, subComponent->mNextWaypointIndex);
		}
		
		subComponent->mPercentBetweenPoints = CalculatePercent();			
		
		if (subComponent->mPercentBetweenPoints >= 0.97f) {
			LOG_DEBUG("WARNING GotoWaypoint was called with very high percent, did something break?");	
			if (!m_PathingStopped) subComponent->mPercentBetweenPoints = 0.98f;
		}	
		
		// start pathing
		StartPathing(true, true);
		
	} else if (m_PathingStopped) {
		StartPathing(true);
	}
}

void MovingPlatformComponent::GotoNextWaypoint(bool stopAtWaypoint, bool immediate) {
	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);
	
	subComponent->mDesiredWaypointIndex = subComponent->mNextWaypointIndex;
	// Just pass it on
	GotoWaypoint(subComponent->mNextWaypointIndex, stopAtWaypoint, immediate);
}

void MovingPlatformComponent::StartPathing(bool forceMove, bool flip) {
    auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);

	// do we have a simple mover?
	if (m_Parent->GetVar<bool>(u"platformIsSimpleMover")) {		
		LOG_DEBUG("StartPathing called on a simple mover");	
		SimpleMove();
		return;
	}

	if (!m_PathingStopped && !forceMove) {
		return;
	} else if (subComponent->mCurrentWaypointIndex == subComponent->mNextWaypointIndex) {
		return;				
	}
	
	// Are we at desired waypoint?
	if (subComponent->mCurrentWaypointIndex == subComponent->mDesiredWaypointIndex) {
		subComponent->mDesiredWaypointIndex = -1;
		if (subComponent->mShouldStopAtDesiredWaypoint) {			
			m_PathingStopped = true;
			subComponent->mState = eMovementPlatformState::Stationary;
			return;				
		}
    }
	
	m_PathingStopped = false;

    m_Parent->CancelCallbackTimers(startPathing_callbackId);

    subComponent->mState = eMovementPlatformState::Stationary;

	// definitions
    NiPoint3 targetPosition;
    uint32_t pathSize;
    PathBehavior behavior;
    if (m_Path != nullptr) {		
		const auto& currentWaypoint = m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex];
        const auto& nextWaypoint = m_Path->pathWaypoints[subComponent->mNextWaypointIndex];

        pathSize = static_cast<uint32_t>(m_Path->pathWaypoints.size() - 1);
        behavior = static_cast<PathBehavior>(m_Path->pathBehavior);
        targetPosition = nextWaypoint.position;
				
		// functionally the same behavior
		if (behavior == PathBehavior::Loop && GetLastWaypointIndex() == 1) {		
			behavior = PathBehavior::Bounce;
		}

		// Already defined, possibly unecessary
        subComponent->mPosition = currentWaypoint.position;
        subComponent->mSpeed = currentWaypoint.speed;
        subComponent->mWaitTime = currentWaypoint.movingPlatform.wait;
    } else {
		// Fallback
        subComponent->mSpeed = 1.0f;
        subComponent->mWaitTime = 2.0f;
        pathSize = 1;
        behavior = PathBehavior::Loop;
        targetPosition = m_Parent->GetPosition() + NiPoint3(0.0f, 10.0f, 0.0f);
    }
	
	// I trust this is here for a reason :)
	if (m_Parent->GetLOT() == 9483)
		behavior = PathBehavior::Bounce;
	
	// calculate travel time
	float totalDistance = Vector3::Distance(targetPosition, subComponent->mPosition);
	subComponent->mLegTotalDistance = totalDistance;
	
	/// -- percent based --
	float remainingPercent = 1.0f - subComponent->mPercentBetweenPoints;
	float travelTime = (remainingPercent * totalDistance) / subComponent->mSpeed;
	if (m_TimeBased)
		travelTime = subComponent->mSpeed;
	
	/// -- distance based -- *it's wrong I know :D
//	float remainingDistance = subComponent->mLegTotalDistance - subComponent->mLegDistanceProgress;	
//	remainingDistance = std::clamp( subComponent->mLegTotalDistance - subComponent->mLegDistanceProgress, 
//		0.0f, subComponent->mLegTotalDistance);	
//	float travelTime = (subComponent->mSpeed > 0.0f) ? remainingDistance / subComponent->mSpeed : 0.0f;	


	auto startLeg = [this, subComponent, flip]() {
		subComponent->mState = eMovementPlatformState::Moving;
		subComponent->mLegStartTime = std::chrono::steady_clock::now();

		if (!flip) StartSounds(true);
		
		Resync();
	};

	float waitTime = subComponent->mWaitTime;	
	if (waitTime <= 0.0f || subComponent->mPercentBetweenPoints > 0.0f) {	
		// just start pathing immediately
		startLeg();
		
		// clear waitTime for destination callback
		waitTime = 0.0f;
	} else {
		// move after wait time
		m_Parent->AddCallbackTimer(subComponent->mWaitTime, startLeg, startPathing_callbackId);		
	}

    // Timer for waypoint reached -> prep & start next path
    m_Parent->AddCallbackTimer(waitTime + travelTime, [this, subComponent, behavior] {	
		const size_t lastIndex = GetLastWaypointIndex();
		
		// Redefine platform data for next path
        subComponent->mCurrentWaypointIndex = subComponent->mNextWaypointIndex;
		subComponent->mLegDistanceProgress = 0.0f;
		subComponent->mPercentBetweenPoints = 0.0f;	
		subComponent->mState = eMovementPlatformState::Stationary;
	
		// send stopping sounds
		StartSounds(false);
		
		// redefine sounds
		auto arriveSound = static_cast<std::string>(m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex].movingPlatform.arriveSound);
		auto departSound = static_cast<std::string>(m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex].movingPlatform.departSound);		
	
		if (!arriveSound.empty())
			subComponent->mStopGUID = arriveSound;
		else
			subComponent->mStopGUID = GeneralUtils::UTF16ToWTF8(m_Parent->GetVar<std::u16string>(u"platformSoundStop"));
		if (!departSound.empty())
			subComponent->mStartGUID = departSound;
		else
			subComponent->mStartGUID = GeneralUtils::UTF16ToWTF8(m_Parent->GetVar<std::u16string>(u"platformSoundStart"));	

		// for scripting
		this->m_Parent->GetScript()->OnWaypointReached(m_Parent, subComponent->mCurrentWaypointIndex);		
		
		
/// 	-- Next waypoint logic --

		// Desired waypoint
		if (subComponent->mDesiredWaypointIndex >= 0 && 
		subComponent->mDesiredWaypointIndex <= GetLastWaypointIndex()) {	
		
			if (subComponent->mCurrentWaypointIndex < subComponent->mDesiredWaypointIndex) {
				subComponent->mNextWaypointIndex = subComponent->mCurrentWaypointIndex + 1; 
				subComponent->mInReverse = false;
			}
			else if (subComponent->mCurrentWaypointIndex > subComponent->mDesiredWaypointIndex) {
				subComponent->mNextWaypointIndex = subComponent->mCurrentWaypointIndex - 1;	
				subComponent->mInReverse = true;	
			} else { // we are at desired waypoint

				if (subComponent->mCurrentWaypointIndex == GetLastWaypointIndex()) {
					if (subComponent->mInReverse || behavior != PathBehavior::Loop) {
						subComponent->mNextWaypointIndex = subComponent->mCurrentWaypointIndex - 1;
						subComponent->mInReverse = true;
					} else {
						subComponent->mNextWaypointIndex = 0;
					}
				} else if (subComponent->mCurrentWaypointIndex == 0) {
					if (!subComponent->mInReverse || behavior != PathBehavior::Loop) {
						subComponent->mNextWaypointIndex = subComponent->mCurrentWaypointIndex + 1;
						subComponent->mInReverse = false;
					} else {
						subComponent->mNextWaypointIndex = GetLastWaypointIndex();
					}
				} else if (subComponent->mInReverse) {
					subComponent->mNextWaypointIndex = subComponent->mCurrentWaypointIndex - 1;	
				} else {
					subComponent->mNextWaypointIndex = subComponent->mCurrentWaypointIndex + 1;	
				}

			}
		
			// Loop
		} else if (behavior == PathBehavior::Loop) {
			if (subComponent->mInReverse) {
					
				subComponent->mNextWaypointIndex =
					(subComponent->mCurrentWaypointIndex == 0) ? 
					lastIndex : subComponent->mCurrentWaypointIndex - 1;	
			}
			else {
				subComponent->mNextWaypointIndex = (subComponent->mCurrentWaypointIndex + 1) % (lastIndex + 1);
			}	
		}
		// Bounce
		else if (behavior == PathBehavior::Bounce) {
			if (subComponent->mInReverse) {
				// moving backward
				if (subComponent->mCurrentWaypointIndex == 0) {
					// time to bounce
					subComponent->mInReverse = false;
					subComponent->mNextWaypointIndex =
						lastIndex > 0 ? 1 : 0;
				}
				else {
					subComponent->mNextWaypointIndex =
						subComponent->mCurrentWaypointIndex - 1;
				}
			} 
			else {
			// Moving forward
				if (subComponent->mCurrentWaypointIndex >= lastIndex) {
					// time to bounce
					subComponent->mInReverse = true;
					subComponent->mNextWaypointIndex = lastIndex > 0 ? lastIndex - 1 : 0;
				}
				else {
					subComponent->mNextWaypointIndex =
						subComponent->mCurrentWaypointIndex + 1;
				}
			}			
		}	
		// Once
		else if (behavior == PathBehavior::Once) {
			const bool atEnd = (!subComponent->mInReverse && subComponent->mCurrentWaypointIndex >= lastIndex);
			const bool atStart = (subComponent->mInReverse && subComponent->mCurrentWaypointIndex == 0);

			if (atEnd) {
				subComponent->mNextWaypointIndex = lastIndex - 1;
				m_PathingStopped = true;

				return;				
			} else if (atStart) {
				subComponent->mNextWaypointIndex = 1;
				m_PathingStopped = true;

				return;				
			} else {
				subComponent->mNextWaypointIndex += subComponent->mInReverse ? -1 : 1;
			}
		}

///		-- End --	


		// Are we at desired waypoint?
        if (subComponent->mCurrentWaypointIndex == subComponent->mDesiredWaypointIndex) {
			subComponent->mDesiredWaypointIndex = -1;
			if (subComponent->mShouldStopAtDesiredWaypoint) {			
				m_PathingStopped = true;

				return;				
			}		
        }
		
		// if we made it here, continue pathing
		this->StartPathing(true);
		
    }, startPathing_callbackId);
}

void MovingPlatformComponent::StopPathing(bool immediate) {
    auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);	
	
	// do we have a simple mover?
	if (m_Parent->GetVar<bool>(u"platformIsSimpleMover")) {		
		LOG("StopPathing will not work on a simple mover, consider attaching a path in hf");
		return;
	}	

	if (m_PathingStopped) {
		return;
	}
	
	if (!immediate) {
		
		subComponent->mDesiredWaypointIndex = subComponent->mNextWaypointIndex;
		subComponent->mShouldStopAtDesiredWaypoint = true;
		return;
	}
	
	// by this point, we are stopped
	m_PathingStopped = true;
	m_Parent->CancelCallbackTimers(startPathing_callbackId);
	
	if (subComponent->mState != eMovementPlatformState::Moving) {
		
		subComponent->mLegDistanceProgress = 0.0f;
		subComponent->mPercentBetweenPoints = 0.0f;		
		subComponent->mState = eMovementPlatformState::Stationary;

		return;
	}

/// -- Timing Related --

	float distance = 0.0f;
	float percent = 0.0f;
	if (subComponent->mLegTotalDistance > 0.0f) {	
	
		// add distance to our progress
		distance = CalculateDistance();	
		subComponent->mLegDistanceProgress += distance;
		subComponent->mLegDistanceProgress = std::clamp(subComponent->mLegDistanceProgress, 0.0f, subComponent->mLegTotalDistance);	
		
		percent = CalculatePercent();
	}
	
/// -- End --

	// set the current percent & state
	subComponent->mPercentBetweenPoints = percent;	
    subComponent->mState = eMovementPlatformState::Stopped;

	// send stopping sounds
	StartSounds(false);

	Resync();
}

void MovingPlatformComponent::SimpleMove() {
	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);
	
	auto waitTime = m_Parent->GetVar<float>(u"platformMoveTime");
	
	// TODO: these actually need to be relative to entity's rotation
	// however, since the counterweights are top side up, it wont matter
	auto moveX = m_Parent->GetVar<float>(u"platformMoveX");
	auto moveY = m_Parent->GetVar<float>(u"platformMoveY");
	auto moveZ = m_Parent->GetVar<float>(u"platformMoveZ");
	NiPoint3 targetPosition = m_Parent->GetPosition() + NiPoint3(moveX, moveY, moveZ);
	
	subComponent->mState = eMovementPlatformState::Moving;
	
	// simpleMover does not seem to work still, remove this if fixed
	m_Parent->SetPosition(targetPosition);
	
	// we can just re-serialize now
	Game::entityManager->SerializeEntity(m_Parent);
	
	// send starting sounds
	StartSounds(true);	
	
	// Timer for waypoint reached
    m_Parent->AddCallbackTimer(waitTime, [this, subComponent] {	

		// send stopping sounds
		StartSounds(false);	
		
		subComponent->mState = eMovementPlatformState::Stationary;

		// for scripting
		this->m_Parent->GetScript()->OnWaypointReached(m_Parent, subComponent->mCurrentWaypointIndex);	
	}, simpleMove_callbackId);
}

float MovingPlatformComponent::CalculateDistance() {
	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);	
	const auto lot = m_Parent->GetLOT();	
	
	auto now = std::chrono::steady_clock::now();
	float elapsed = std::chrono::duration<float>(now - subComponent->mLegStartTime).count();
	

	// wouldn't be my kinda code w/o shit like this :D
	// helps with repeated calls (earth garden pillars), but could be noticeable over long distances
//	if (lot == 14447 || lot == 14449) {
//		if (!subComponent->mInReverse)
//			elapsed = elapsed * 1.1f;
//		else
//			elapsed = elapsed * 0.9f;
//	}
	

	float distance = elapsed * subComponent->mSpeed;
	// subtract if reverse
	if (subComponent->mInReverse) distance = -distance;
		
	// if negative distance, platform should be back tracking
	if (subComponent->mLegDistanceProgress + distance < 0.0f) distance += subComponent->mLegTotalDistance;
	
	return distance;
}

float MovingPlatformComponent::CalculatePercent() {
	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);
	
    if (!subComponent || subComponent->mLegTotalDistance <= 0.0f)
        return 0.0f;

    float percent = subComponent->mLegDistanceProgress / subComponent->mLegTotalDistance;
    percent = std::clamp(percent, 0.0f, 1.0f);

    if (subComponent->mInReverse && percent > 0.0f)
        percent = 1.0f - percent;	
 
    return percent;
}

void MovingPlatformComponent::Resync(const SystemAddress& sysAddr) {
	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);
	
//	LOG_DEBUG("SendPlatformResync called for object id %llu with LOT %i -> current=%u next=%u desired=%u percent=%.3f state=%d",
//		m_Parent->GetObjectID(),
//		m_Parent->GetLOT(),
//      subComponent->mCurrentWaypointIndex,
//      subComponent->mNextWaypointIndex,
//      subComponent->mDesiredWaypointIndex,
//      subComponent->mPercentBetweenPoints,
//      subComponent->mState);
	
    GameMessages::SendPlatformResync(
        m_Parent,
        sysAddr,
        true,
        subComponent->mCurrentWaypointIndex,
        subComponent->mNextWaypointIndex,
        subComponent->mNextWaypointIndex,
        subComponent->mState,
        subComponent->mPercentBetweenPoints,
		subComponent->mWaitTime,
		subComponent->mLegDistanceProgress / subComponent->mSpeed,		
		subComponent->mInReverse);	

	
}

void MovingPlatformComponent::StartSounds(bool starting) {
	auto* subComponent = static_cast<MoverSubComponent*>(m_MoverSubComponent);
	
	auto startSound = subComponent->mStartGUID;
	auto stopSound = subComponent->mStopGUID;
	auto travelSound = subComponent->mTravelGUID;
	
	// platform is starting
	if (starting) {
		if (!startSound.empty())
			GameMessages::SendPlayNDAudioEmitter(m_Parent, UNASSIGNED_SYSTEM_ADDRESS, startSound);
		if (!travelSound.empty())
			GameMessages::SendPlayNDAudioEmitter(m_Parent, UNASSIGNED_SYSTEM_ADDRESS, travelSound);

	// platform is stopping
	} else {
		if (!travelSound.empty())
			GameMessages::SendStopNDAudioEmitter(m_Parent, UNASSIGNED_SYSTEM_ADDRESS, travelSound);
		if (!stopSound.empty())
			GameMessages::SendPlayNDAudioEmitter(m_Parent, UNASSIGNED_SYSTEM_ADDRESS, stopSound);
	}
}

void MovingPlatformComponent::SetSerialized(bool value) {
	m_Serialize = value;
}

bool MovingPlatformComponent::GetNoAutoStart() const {
	return m_NoAutoStart;
}

void MovingPlatformComponent::SetNoAutoStart(const bool value) {
	m_NoAutoStart = value;
}

void MovingPlatformComponent::WarpToWaypoint(size_t index) {
	const auto& waypoint = m_Path->pathWaypoints[index];

	// Just pass it on
	GotoWaypoint(index);
}

size_t MovingPlatformComponent::GetLastWaypointIndex() const {
	return m_Path->pathWaypoints.size() - 1;
}

MoverSubComponent* MovingPlatformComponent::GetMoverSubComponent() const {
	return static_cast<MoverSubComponent*>(m_MoverSubComponent);
}
