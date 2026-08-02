#include "MovingPlatformComponent.h"
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

//------------- MovingPlatformComponent below --------------

MovingPlatformComponent::MovingPlatformComponent(Entity* parent, const int32_t componentID, const std::string& pathName) : Component(parent, componentID) {
	// create submover
	subComponent = new MoverSubComponent(m_Parent->GetDefaultPosition());

	// sounds
	subComponent->mStartGUID = GeneralUtils::UTF16ToWTF8(m_Parent->GetVar<std::u16string>(u"platformSoundStart"));
	subComponent->mStopGUID = GeneralUtils::UTF16ToWTF8(m_Parent->GetVar<std::u16string>(u"platformSoundStop"));
	subComponent->mTravelGUID = GeneralUtils::UTF16ToWTF8(m_Parent->GetVar<std::u16string>(u"platformSoundTravel"));
	
	// start pathing on load
	m_NoAutoStart = !m_Parent->GetVar<bool>(u"startPathingOnLoad");
	
	// velocity
	subComponent->mVelocity = NiPoint3Constant::ZERO;
	subComponent->mAngularVelocity = NiPoint3Constant::ZERO;

	// do we have a simple mover?
	if (m_Parent->GetVar<bool>(u"platformIsSimpleMover")) {	
		m_MoverSubComponentType = eMoverSubComponentType::simpleMover;	
		m_TimeBased = true;
		
		subComponent->mPosition = m_Parent->GetPosition();
		subComponent->mRotation = m_Parent->GetRotation();	
		
		// 2 point path
		if (m_Parent->GetVar<bool>(u"platformStartAtEnd")) {
			SimpleMovePrepareForward(false);
		} else {
			SimpleMovePrepareForward(true);
		}
		
		// define waypoints
		auto moveX = m_Parent->GetVar<float>(u"platformMoveX");
		auto moveY = m_Parent->GetVar<float>(u"platformMoveY");
		auto moveZ = m_Parent->GetVar<float>(u"platformMoveZ");

		// waypoint 0
		subComponent->mSimpleMoverStartPosition = subComponent->mPosition;
		
		// waypoint 1
		subComponent->mSimpleMoverEndPosition.x = subComponent->mSimpleMoverStartPosition.x + moveX;
		subComponent->mSimpleMoverEndPosition.y = subComponent->mSimpleMoverStartPosition.y + moveY;
		subComponent->mSimpleMoverEndPosition.z = subComponent->mSimpleMoverStartPosition.z + moveZ;

		return;	
		
	} else m_MoverSubComponentType = eMoverSubComponentType::mover;

	m_PathName = GeneralUtils::ASCIIToUTF16(pathName);
	m_Path = Game::zoneManager->GetZone()->GetPath(pathName);

	m_TimeBased = false;
	if (m_Path && m_Path->pathType == PathType::MovingPlatform)
		m_TimeBased = static_cast<bool>(m_Path->movingPlatform.timeBasedMovement);

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
			auto start = m_Parent->GetVarAs<int32_t>(u"attached_path_start");
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


	// if we serlialize later, clients should not start pathing without server
	//	m_NoAutoStart = true;

}

MovingPlatformComponent::~MovingPlatformComponent() {
	delete subComponent;
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

	const auto hasPlatform = subComponent != nullptr;
	outBitStream.Write<bool>(hasPlatform);

	if (hasPlatform) {
		outBitStream.Write(m_MoverSubComponentType);

		if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
			subComponent->Serialize(outBitStream, bIsInitialUpdate, true, m_Parent->GetPosition(), m_Parent->GetRotation());
		} else {
			subComponent->Serialize(outBitStream, bIsInitialUpdate);
		}
	}
}

void MovingPlatformComponent::OnQuickBuildInitilized() {
	// include if needed
	// StopPathing();
}

void MovingPlatformComponent::OnCompleteQuickBuild() {
	if (m_NoAutoStart) return;

	StartPathing();
}

void MovingPlatformComponent::Update(float deltaTime) {

	// are we moving?
	if (subComponent->mState != eMovementPlatformState::Moving || m_PathingStopped) {
		// increment idle time
		subComponent->mIdleTimeElapsed += deltaTime;
		// return;

	} else {
		// increment moved time
		subComponent->mMoveTimeElapsed += deltaTime;

		// update percent
		float newPercent = std::clamp(subComponent->mMoveTimeElapsed / subComponent->mTotalMoveTime, 0.0f, 1.0f);
		subComponent->mPercentBetweenPoints = newPercent;
	}

	// calculate pose & velocity
	Vector3 currentPosition = CalculatePosition();
	NiQuaternion currentRotation = CalculateRotation();
	Vector3 velocity = CalculateVelocity();
	Vector3 angularVelocity = CalculateAngularVelocity();

	// store pose & velocity
	// we don't actually use these, but they could be helpful for future scripts
	subComponent->mPosition = currentPosition;
	subComponent->mRotation = currentRotation;
	subComponent->mVelocity = velocity;
	subComponent->mAngularVelocity = angularVelocity;

	// Update the physics entity pose for phantom physics
	// Could be done differently, this just seems the most straightforward approach
	auto* phantomPhysicsComponent = m_Parent->GetComponent<PhantomPhysicsComponent>();
	if (phantomPhysicsComponent) {
		dpEntity* phantomEntity = phantomPhysicsComponent->GetdpEntity();
		phantomEntity->SetPosition(currentPosition);
	}

	// TODO: update proximity pose
//	auto* proximityMonitorComponent = m_Parent->GetComponent<ProximityMonitorComponent>();
//	if (proximityMonitorComponent) {
//		dpEntity* proximityEntity = proximityMonitorComponent->GetdpEntity();
//	}



//	auto currentPosition = CalculatePosition();
//	LOG_DEBUG(
//		"Platform %llu finalPercent=%.3f position=(%.3f, %.3f, %.3f)",
//		m_Parent->GetObjectID(),
//		subComponent->mPercentBetweenPoints,
//		currentPosition.x,
//		currentPosition.y,
//		currentPosition.z);

}

void MovingPlatformComponent::GotoWaypoint(uint32_t index, bool stopAtWaypoint, bool immediate) {

	// do we have a simple mover?
	if (m_Parent->GetVar<bool>(u"platformIsSimpleMover")) {		
		LOG_DEBUG("GotoWaypoint called on a simple mover");
		if (index == subComponent->mCurrentWaypointIndex) {
			LOG_DEBUG("simple mover already at desired waypoint");
			return;
			
		} else if (subComponent->mState == eMovementPlatformState::Moving)
			return; // only one place to go :)
		else if (index == 0) 
			SimpleMovePrepareForward(false);
		else 
			SimpleMovePrepareForward(true);

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

    else if (subComponent->mDesiredWaypointIndex == index) {
		if (m_PathingStopped) {
			// we just need a start
			StartPathing(true);
		}
		return;	
	}

	subComponent->mDesiredWaypointIndex = index;

	// finish leg, then path to waypoint	
	if (!immediate) {	

		if (m_PathingStopped) StartPathing(true);
		return;
	}		

	else if ((index <= subComponent->mCurrentWaypointIndex && !subComponent->mInReverse) || 
		(index >= subComponent->mCurrentWaypointIndex && subComponent->mInReverse)) {		
		// we need to flip the direction

		// looping path behavior isn't great for this scenario, temporary fix
		PathBehavior behavior = static_cast<PathBehavior>(m_Path->pathBehavior);
		if (behavior != PathBehavior::Loop) {
			subComponent->mInReverse = !subComponent->mInReverse;
			std::swap(subComponent->mCurrentWaypointIndex, subComponent->mNextWaypointIndex);
		}
		
		// flip the percent
		if (subComponent->mPercentBetweenPoints > 0.0f) 
			subComponent->mPercentBetweenPoints = 1.0f - subComponent->mPercentBetweenPoints;
		
		// flip the move times
		const auto& currentWaypoint = m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex];
		float totalMoveTime = currentWaypoint.speed;
		if (!m_TimeBased) {
			const auto& nextWaypoint = m_Path->pathWaypoints[subComponent->mNextWaypointIndex];
			float totalDistance = Vector3::Distance(currentWaypoint.position, nextWaypoint.position);
			totalMoveTime = totalDistance / currentWaypoint.speed;
		}
		
		subComponent->mMoveTimeElapsed = totalMoveTime * subComponent->mPercentBetweenPoints;

		// start pathing
		StartPathing(true, true);

	} else if (m_PathingStopped) {
		StartPathing(true);
	}
}

void MovingPlatformComponent::GotoNextWaypoint(bool stopAtWaypoint, bool immediate) {
	
	subComponent->mDesiredWaypointIndex = subComponent->mNextWaypointIndex;
	// Just pass it on
	GotoWaypoint(subComponent->mNextWaypointIndex, stopAtWaypoint, immediate);
}

void MovingPlatformComponent::StartPathing(bool forceMove, bool flip) {

	// do we have a simple mover?
	if (m_Parent->GetVar<bool>(u"platformIsSimpleMover")) {		
		LOG_DEBUG("StartPathing called on a simple mover");
		SimpleMove();
		return;

	} else if (m_Path == nullptr) return;

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
	NiPoint3 currentPosition;
    NiPoint3 targetPosition;
    uint32_t pathSize;
    PathBehavior behavior;
    if (m_Path != nullptr) {		
		const auto& currentWaypoint = m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex];
        const auto& nextWaypoint = m_Path->pathWaypoints[subComponent->mNextWaypointIndex];

        pathSize = static_cast<uint32_t>(m_Path->pathWaypoints.size() - 1);
        behavior = static_cast<PathBehavior>(m_Path->pathBehavior);
		currentPosition = currentWaypoint.position;
        targetPosition = nextWaypoint.position;
				
		// functionally the same behavior
		if (behavior == PathBehavior::Loop && GetLastWaypointIndex() == 1) {		
			behavior = PathBehavior::Bounce;
		}

		// Already defined, possibly unecessary
        subComponent->mSpeed = currentWaypoint.speed;
        subComponent->mWaitTime = currentWaypoint.movingPlatform.wait;
    } else {
		// Fallback
        subComponent->mSpeed = 1.0f;
        subComponent->mWaitTime = 2.0f;
        pathSize = 1;
        behavior = PathBehavior::Once;
		currentPosition = m_Parent->GetPosition();
        targetPosition = m_Parent->GetPosition() + NiPoint3(0.0f, 10.0f, 0.0f);
    }
	
	// I trust this is here for a reason :)
	if (m_Parent->GetLOT() == 9483)
		behavior = PathBehavior::Bounce;
	
	// calculate move times
	float totalMoveTime = subComponent->mSpeed;
	if (!m_TimeBased) {
		float totalDistance = Vector3::Distance(currentPosition, targetPosition);
		totalMoveTime = totalDistance / subComponent->mSpeed;
	}
	
	subComponent->mTotalMoveTime = totalMoveTime;

	auto startLeg = [this, flip]() {
		subComponent->mState = eMovementPlatformState::Moving;

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
		// reset elapsed times
		subComponent->mIdleTimeElapsed = 0.0f;
		subComponent->mMoveTimeElapsed = 0.0f;

		// reset percent (not needed)
		subComponent->mPercentBetweenPoints = 0.0f;

		// move after wait time
		m_Parent->AddCallbackTimer(subComponent->mWaitTime, startLeg, startPathing_callbackId);		
	}

	float remainingMoveTime = totalMoveTime - (totalMoveTime * subComponent->mPercentBetweenPoints);

    // Timer for waypoint reached -> prep & start next path
    m_Parent->AddCallbackTimer(waitTime + remainingMoveTime, [this, behavior] {	
		const size_t lastIndex = GetLastWaypointIndex();

		// Redefine platform data for next path
        subComponent->mCurrentWaypointIndex = subComponent->mNextWaypointIndex;
		subComponent->mIdleTimeElapsed = 0.0f;
		subComponent->mMoveTimeElapsed = 0.0f;
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

	// set the current state
    subComponent->mState = eMovementPlatformState::Stopped;

	// send stopping sounds
	StartSounds(false);

	Resync();
}

void MovingPlatformComponent::SimpleMove(bool serialize) {
	auto moveTime = m_Parent->GetVar<float>(u"platformMoveTime");

	subComponent->mTotalMoveTime = moveTime;
	subComponent->mState = eMovementPlatformState::Moving;

	// we can just re-serialize now
	if (serialize) {
		m_Serialize = true;
		Game::entityManager->SerializeEntity(m_Parent);
	}

	// send starting sounds
	StartSounds(true);

	// Timer for waypoint reached
    m_Parent->AddCallbackTimer(moveTime, [this, serialize] {
		// send stopping sounds
		StartSounds(false);	

		subComponent->mState = eMovementPlatformState::Stationary;

		if (subComponent->mCurrentWaypointIndex == 0)
			SimpleMovePrepareForward(false);
		else
			SimpleMovePrepareForward(true);

		// probably not needed
//		if (serialize) {
//			m_Serialize = true;
//			Game::entityManager->SerializeEntity(m_Parent);
//		}

		// for scripting
		this->m_Parent->GetScript()->OnWaypointReached(m_Parent, subComponent->mCurrentWaypointIndex);	
	}, simpleMove_callbackId);
}

void MovingPlatformComponent::SimpleMovePrepareForward(bool forward) {
	
	if (forward) {
		subComponent->mCurrentWaypointIndex = 0;
		subComponent->mNextWaypointIndex = 1;
		subComponent->mInReverse = false;		
	} else {
		subComponent->mCurrentWaypointIndex = 1;
		subComponent->mNextWaypointIndex = 0;
		subComponent->mInReverse = true;		
	}
	
	subComponent->mPercentBetweenPoints = 0.0f;

	subComponent->mIdleTimeElapsed = 0.0f;
	subComponent->mMoveTimeElapsed = 0.0f;

	subComponent->mDesiredWaypointIndex = subComponent->mNextWaypointIndex;
}

void MovingPlatformComponent::Resync(const SystemAddress& sysAddr) {

//	LOG_DEBUG("SendPlatformResync called for object id %llu with LOT %i -> current=%u next=%u desired=%u percent=%.3f state=%d",
//		m_Parent->GetObjectID(),
//		m_Parent->GetLOT(),
//      subComponent->mCurrentWaypointIndex,
//      subComponent->mNextWaypointIndex,
//      subComponent->mDesiredWaypointIndex,
//      subComponent->mPercentBetweenPoints,
//      subComponent->mState);

	if (m_MoverSubComponentType == eMoverSubComponentType::simpleMover) {
		// we can just re-serialize for the specific player
		m_Serialize = true;
		Game::entityManager->SerializeEntity(m_Parent, sysAddr);

	} else {
		GameMessages::SendPlatformResync(
			m_Parent,
			sysAddr,
			true,
			subComponent->mCurrentWaypointIndex,
			subComponent->mNextWaypointIndex,
			subComponent->mNextWaypointIndex,
			subComponent->mState,
			subComponent->mPercentBetweenPoints,
			subComponent->mIdleTimeElapsed,
			subComponent->mMoveTimeElapsed,		
			subComponent->mInReverse);	
	}
}

void MovingPlatformComponent::StartSounds(bool starting) {
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

Vector3 MovingPlatformComponent::CalculatePosition() {
	auto& currentWaypointPosition = subComponent->mSimpleMoverStartPosition;
	auto& nextWaypointPosition = subComponent->mSimpleMoverEndPosition;	

	if (m_MoverSubComponentType != eMoverSubComponentType::simpleMover) {
		if (!m_Path) return m_Parent->GetPosition();
		currentWaypointPosition = m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex].position;
		nextWaypointPosition = m_Path->pathWaypoints[subComponent->mNextWaypointIndex].position;

	} else if (subComponent->mInReverse) {
		currentWaypointPosition = subComponent->mSimpleMoverEndPosition;
		nextWaypointPosition = subComponent->mSimpleMoverStartPosition;	
	}

	float percent = subComponent->mPercentBetweenPoints;
	
	Vector3 finalPosition = currentWaypointPosition + (nextWaypointPosition - currentWaypointPosition) * percent;
	return finalPosition;
}

NiQuaternion MovingPlatformComponent::CalculateRotation() {
	if (!m_Path) return m_Parent->GetRotation();

	float percent = subComponent->mPercentBetweenPoints;

	const auto& currentWaypointRotation = m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex].rotation;
	const auto& nextWaypointRotation = m_Path->pathWaypoints[subComponent->mNextWaypointIndex].rotation;

	NiQuaternion finalRotation = glm::slerp(currentWaypointRotation, nextWaypointRotation, percent);
	return finalRotation;
}

Vector3 MovingPlatformComponent::CalculateVelocity() {
	if (subComponent->mState != eMovementPlatformState::Moving || m_PathingStopped || !m_Path) 
		return NiPoint3(0.0f, 0.0f, 0.0f);

	const auto& currentWaypointPosition = m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex].position;
	const auto& nextWaypointPosition = m_Path->pathWaypoints[subComponent->mNextWaypointIndex].position;
	Vector3 displacement = nextWaypointPosition - currentWaypointPosition;

	Vector3 finalVelocity = NiPoint3(0.0f, 0.0f, 0.0f);
	if (!m_TimeBased) {
		Vector3 direction = displacement.Unitize();
		finalVelocity = direction * subComponent->mSpeed;

	} else if (subComponent->mTotalMoveTime <= 0.0f) {
		return finalVelocity;
		
	} else finalVelocity = displacement / subComponent->mTotalMoveTime;

	return finalVelocity;
}

Vector3 MovingPlatformComponent::CalculateAngularVelocity() {
	if (!m_Path || subComponent->mState != eMovementPlatformState::Moving || m_PathingStopped)
		return NiPoint3(0.0f, 0.0f, 0.0f);

	const auto& currentWaypointRotation = m_Path->pathWaypoints[subComponent->mCurrentWaypointIndex].rotation;

	const auto& nextWaypointRotation = m_Path->pathWaypoints[subComponent->mNextWaypointIndex].rotation;

	NiQuaternion delta = glm::normalize(
		nextWaypointRotation * glm::inverse(currentWaypointRotation)
	);

	float angle = glm::angle(delta);

	if (angle <= 0.001f)
		return NiPoint3(0.0f, 0.0f, 0.0f);

	Vector3 axis = glm::axis(delta);

	float moveTime = subComponent->mTotalMoveTime;

	if (moveTime <= 0.0f)
		return NiPoint3(0.0f, 0.0f, 0.0f);

	return axis * (angle / moveTime);
}

Vector3 MovingPlatformComponent::GetPosition() {
	if (!subComponent) return NiPoint3(0.0f, 0.0f, 0.0f);
	return subComponent->mPosition;
}

NiQuaternion MovingPlatformComponent::GetRotation() {
	if (!subComponent) return m_Parent->GetRotation();
	return subComponent->mRotation;
}

Vector3 MovingPlatformComponent::GetVelocity() {
	if (!subComponent) return NiPoint3(0.0f, 0.0f, 0.0f);
	return subComponent->mVelocity;
}

Vector3 MovingPlatformComponent::GetAngularVelocity() {
	if (!subComponent) return NiPoint3(0.0f, 0.0f, 0.0f);
	return subComponent->mAngularVelocity;
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

eMoverSubComponentType MovingPlatformComponent::GetSubMoverType() const {
	return m_MoverSubComponentType;
}

void MovingPlatformComponent::WarpToWaypoint(size_t index) {
	// Just pass it on
	GotoWaypoint(index);
}

size_t MovingPlatformComponent::GetLastWaypointIndex() const {
	if (m_Parent->GetVar<bool>(u"platformIsSimpleMover"))
		return 1;
	else
		return m_Path->pathWaypoints.size() - 1;
}

MoverSubComponent* MovingPlatformComponent::GetMoverSubComponent() const {
	return subComponent;
}
