#include "NjFireRocks.h"
#include "ProximityMonitorComponent.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "MovingPlatformComponent.h"
#include "EntityInfo.h"
#include "GameMessages.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"

void NjFireRocks::OnStartup(Entity* self) {			
	// set defaults
	if (self->GetVar<std::u16string>(u"RockManagerGroup") != u"LavaRocks02Controller") {
		self->SetVar<std::u16string>(u"RockManagerGroup", u"FireTransRocksManager");
	}
	
	// start pathing on load
	auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
	if (!movingPlatformComponent) return;
	movingPlatformComponent->SetNoAutoStart(false);	
}

void NjFireRocks::OnWaypointReached(Entity* self, uint32_t waypointIndex) {
	if (waypointIndex == 1) {
		const auto managerGroup = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"RockManagerGroup"));
		const auto managerEntity = Game::entityManager->GetEntitiesInGroup(managerGroup);
		
		for (auto* manager : managerEntity) {
			manager->NotifyObject(self, "PlatformArrived");
		}			
	}
}

