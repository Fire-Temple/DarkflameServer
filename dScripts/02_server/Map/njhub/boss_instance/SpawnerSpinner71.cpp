#include "SpawnerSpinner71.h"
#include "ProximityMonitorComponent.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "MovingPlatformComponent.h"
#include "EntityInfo.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"
#include "dZoneManager.h"
#include "SoundTriggerComponent.h"

void SpawnerSpinner71::OnStartup(Entity* self) {
	RenderComponent::PlayAnimation(self, u"idle");
	self->SetVar<int>(u"SpinnerProgress", 0);
}

void SpawnerSpinner71::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1,
	int32_t param2) {	
	auto progress = self->GetVar<int>(u"SpinnerProgress");
	
	if (name == "SpinnerDeactivated") {
		if (progress == 7) {	
//			start audio sequence for proper players
			self->SetNetworkVar(u"flourishcue", 1);
			self->AddTimer("PlayerFlourish", 5.5);
		
//			start spawner	
			self->AddTimer("SpawnerActivate", 4);		
		}
		self->SetVar<int>(u"SpinnerProgress", progress + 1);	
	}
}

void SpawnerSpinner71::OnTimerDone(Entity* self, std::string timerName) {

	if (timerName == "PlayerFlourish") {	
		self->SetNetworkVar(u"triggerflourish", 1);		
		
	} else if (timerName == "MoveUp") {	
		auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();	
		
		movingPlatformComponent->GotoWaypoint(1);
		RenderComponent::PlayAnimation(self, u"up");
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{7f770ade-b84c-46ad-b3ae-bdbace5985d4}");
		
	} else if (timerName == "SpawnVultures") {	
		const auto spawnNw = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"spawnNw1"));	
		auto spawners = Game::zoneManager->GetSpawnersByName(spawnNw);	
		
		if (!spawners.empty()) {
			auto* spawner = spawners.front();
			spawner->Activate();
		}
		
	} else if (timerName == "SpawnerActivate") {	

		self->AddTimer("MoveUp", 0.9f);
		self->AddTimer("SpawnVultures", 3);
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 10102, u"create", "create");		
	}
}