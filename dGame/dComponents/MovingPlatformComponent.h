#ifndef MOVINGPLATFORMCOMPONENT_H
#define MOVINGPLATFORMCOMPONENT_H

#include "RakNetTypes.h"
#include "NiPoint3.h"
#include <string>

#include "dCommonVars.h"
#include "EntityManager.h"
#include "Component.h"
#include "eMovementPlatformState.h"
#include "eReplicaComponentType.h"

#include <chrono>

class Path;

 /**
  * Different types of available platforms
  */
enum class eMoverSubComponentType : uint32_t {
	mover = 4,

	/**
	 * Used in NJ
	 */
	 simpleMover = 5,
};

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
	 * The scheduled waypoint this platform currently wants to traverse to
	 */
	int32_t mScheduledWaypoint = 0;	

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
	 * Timestamp of when the platform began moving
	 */	
	std::chrono::steady_clock::time_point mLegStartTime{};

	/**
	 * The total travel distance for the platform
	 */
	float mLegTotalDistance;
	
	/**
	 * Estimated distance traveled for the platform; always forward facing
	 */	
	float mLegDistanceProgress;
		
	/**
	 * Finish the current leg before stopping
	 */
	bool mFinishLeg;
	
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
	 * The timer that handles the time before stopping idling and continue platform movement
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


/**
 * Represents entities that may be moving platforms, indicating how they should move through the world.
 * NOTE: the logic in this component hardly does anything, apparently the client can figure most of this stuff out
 * if you just serialize it correctly, resulting in smoother results anyway. Don't be surprised if the exposed APIs
 * don't at all do what you expect them to as we don't instruct the client of changes made here.
 * ^^^ Trivia: This made the red blocks platform and property platforms a pain to implement.
 */
class MovingPlatformComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::MOVING_PLATFORM;

	MovingPlatformComponent(Entity* parent, const int32_t componentID, const std::string& pathName);
	~MovingPlatformComponent() override;

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;

	/**
	 * Stops all pathing, called when an entity starts a quick build associated with this platform
	 */
	void OnQuickBuildInitilized();

	/**
	 * Starts the pathing, called when an entity completed a quick build associated with this platform
	 */
	void OnCompleteQuickBuild();

	/**
	 * Instructs the moving platform to go to some waypoint
	 * @param index the index of the waypoint
	 * @param stopAtWaypoint determines if the platform should stop at the waypoint
	 */
	void GotoWaypoint(uint32_t index, bool stopAtWaypoint = true, bool immediate = true);
	
	/**
	 * Instructs the moving platform to go to the next waypoint
	 * @param stopAtWaypoint determines if the platform should stop at the waypoint
	 */
	void GotoNextWaypoint(bool stopAtWaypoint = true, bool immediate = true);	

	/**
	 * Starts the pathing of this platform, setting appropriate waypoints and speeds
	 */
	void StartPathing(bool forceMove = false, bool flip = false);

	/**
	 * Stops the platform from moving, waiting for it to be activated again.
	 */
	void StopPathing(bool immediate = true);

	/**
	 * Starts pathing for simple movers (NJ)
	 */
	void SimpleMove();

	/**
	 * Turn time traveled into distance; always forward facing
	 */
	float CalculateDistance();

	/**
	 * Turn progress times into percent for precise pathing
	 */
	float CalculatePercent();
	
	/**
	 * Send resync with server values
	 */	
	void Resync(const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS);

	/**
	 * Determines if the entity should be serialized on the next update
	 * @param value whether to serialize the entity or not
	 */
	void SetSerialized(bool value);

	/**
	 * Sets appropriate sounds for the platform
	 */
	void StartSounds(bool starting);
	
	/**
	 * Returns if this platform will start automatically after spawn
	 * @return if this platform will start automatically after spawn
	 */
	bool GetNoAutoStart() const;

	/**
	 * Sets the auto start value for this platform
	 * @param value the auto start value to set
	 */
	void SetNoAutoStart(bool value);

	/**
	 * Warps the platform to a waypoint index, skipping its current path
	 * @param index the index to go to
	 */
	void WarpToWaypoint(size_t index);

	/**
	 * Returns the waypoint this platform was previously at
	 * @return the waypoint this platform was previously at
	 */
	size_t GetLastWaypointIndex() const;

	/**
	 * Returns the sub component that actually defines how the platform moves around (speeds, etc).
	 * @return the sub component that actually defines how the platform moves around
	 */
	MoverSubComponent* GetMoverSubComponent() const;
	
	/**
	 * Whether the platform shouldn't auto start
	 */
	bool m_NoAutoStart;	

private:

	/**
	 * The path this platform is currently on
	 */
	const Path* m_Path = nullptr;

	/**
	 * The name of the path this platform is currently on
	 */
	std::u16string m_PathName;

	/**
	 * Whether the platform has stopped pathing
	 */
	bool m_PathingStopped = true;
	
	/**
	 * Is the platform direclty pathing from GotoWaypoint
	 */
	bool m_IsDirectPathing = false;	
	
	/**
	 * Whether the platform has a path
	 */
	bool m_HasPath = true;	

	/**
	 * The type of the subcomponent
	 */
	eMoverSubComponentType m_MoverSubComponentType;

	/**
	 * The mover sub component that belongs to this platform
	 */
	void* m_MoverSubComponent;

	/**
	 * If speed is actually time in seconds
	 */
	bool m_TimeBased;	
	
	/**
	 * Whether to serialize the entity on the next update
	 */
	bool m_Serialize = true;
	
	/**
	 * hardcoded CallbackTimer ids, component id + incremental per function or whatever we need
	 */
	uint32_t startPathing_callbackId = 2501;	
	uint32_t simpleMove_callbackId = 2502;
};

#endif // MOVINGPLATFORMCOMPONENT_H
