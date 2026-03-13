//Used for LOT 32050 to not forget

#include "MillstoneEvent.h"
#include "ProximityMonitorComponent.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "MovingPlatformComponent.h"
#include "EntityInfo.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"

void MillstoneEvent::OnStartup(Entity* self) {
	self->SetNetworkVar(u"bIsInUse", false);
	self->SetVar(u"bActive", true);
}

void MillstoneEvent::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1,
	int32_t param2) {
	const auto attachedPath = self->GetVar<std::u16string>(u"attached_path");
	auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
	
	if (name == "SpinnerDeactivated") {	
		if (attachedPath == u"MillstoneA") {
			movingPlatformComponent->StartPathing();
			return;
		}	
		
		movingPlatformComponent->GotoNextWaypoint();
	}	
}
