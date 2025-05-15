#pragma once
#include "CppScripts.h"

class ZoneGfPropertyMed : virtual public CppScripts::Script {
public:

	void SetGameVariables(Entity* self);
	void CheckForOwner(Entity* self);
	void OnStartup(Entity* self) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;
	void OnPlayerLoaded(Entity* self, Entity* player) override;
	void OnZonePropertyRented(Entity* self, Entity* renter) override;	
	void OnZonePropertyModelPlaced(Entity* self, Entity* player) override;	
	void OnPlayerExit(Entity* self, Entity* player) override;

	
	static LWOOBJID GetOwner();	

	void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1 = 0, int32_t param2 = 0) override;
	
	void OnTimerDone(Entity* self, std::string timerName) override;


protected:


	// GUIDs

	// Groups
	std::u16string PropertyPlaqueGroup = u"PropertyPlaqueGroup";
	std::u16string PropertyVendorGroup = u"PropertyVendorGroup";
	std::u16string PropertyBorderGroup = u"PropertyBorderGroup";
	std::u16string ImagOrbGroup = u"ImagOrbGroup";
	std::u16string ClaimMarkerGroup = u"ClaimMarkerGroup";

	// Spawners
	std::u16string EnemiesSpawner = u"EnemiesSpawner";
	std::u16string PropObjsSpawner = u"PropObjsSpawner";
	std::u16string ImageOrbSpawner = u"ImageOrbSpawner";
	std::u16string ClaimMarkerSpawner = u"ClaimMarkerSpawner";
	std::u16string BehaviorObjsSpawner = u"BehaviorObjsSpawner";

	//Flags / constants
	std::u16string brickLinkMissionIDFlag = u"brickLinkMissionIDFlag";
	std::u16string placedModelFlag = u"placedModelFlag";
	std::u16string defeatedProperyFlag = u"defeatedProperyFlag";
	std::u16string passwordFlag = u"passwordFlag";
	std::u16string orbIDFlag = u"orbIDFlag";
	std::u16string behaviorQBID = u"behaviorQBID";

	// Variables
	std::u16string PlayerIDVariable = u"playerID";
	std::u16string CollidedVariable = u"collided";
	std::u16string PropertyOwnerVariable = u"PropertyOwner";
	std::u16string PropertyOwnerIDVariable = u"PropertyOwnerID";
	std::u16string FXObjectsGoneVariable = u"FXObjectGone";
	std::u16string RenterVariable = u"renter";
	std::u16string UnclaimedVariable = u"unclaimed";

	// Events
	std::string CheckForPropertyOwnerEvent = "CheckForPropertyOwner";

	// Timers
	std::string StartOrbTimer = "startOrb";
	std::string StartQuickbuildTimer = "startQuickbuild";
	std::string ShowVendorTimer = "ShowVendor";
	std::string BoundsVisOnTimer = "BoundsVisOn";
	std::string RunPlayerLoadedAgainTimer = "runPlayerLoadedAgain";
};
