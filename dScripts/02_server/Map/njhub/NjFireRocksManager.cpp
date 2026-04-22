#include "NjFireRocksManager.h"
#include "MovingPlatformComponent.h"

#include <algorithm>

void NjFireRocksManager::OnStartup(Entity* self) {

	// set defaults	
	if (self->GetVar<std::u16string>(u"RockGroup") != u"LavaRocks02") {
		self->SetVar<std::u16string>(u"RockGroup", u"FireTransRocks");
	}
	if (self->GetVar<int32_t>(u"NumberOfRocks") != 2) {
		self->SetVar<int32_t>(u"NumberOfRocks", 3);
	}
	
	self->SetVar<int32_t>(u"ArrivedRocks", 0);
}

void NjFireRocksManager::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1,
	int32_t param2) {	
	if (name == "PlatformArrived") {
		const auto totalRocks = self->GetVar<int32_t>(u"NumberOfRocks");
		auto arrivedRocks = self->GetVar<int32_t>(u"ArrivedRocks") + 1;

		self->SetVar<int32_t>(u"ArrivedRocks", arrivedRocks);

		if (arrivedRocks >= totalRocks) {
			LOG_DEBUG("NjFireRocksManager.cpp final rock arrived, start pathing");
			self->SetVar<int32_t>(u"ArrivedRocks", 0);		

			const auto rockGroup = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"RockGroup"));
			const auto rockEntities = Game::entityManager->GetEntitiesInGroup(rockGroup);

			// start pathing
			for (auto* rock : rockEntities) {	
				if (rock != sender) {
					auto* movingPlatformComponent = rock->GetComponent<MovingPlatformComponent>();
					if (movingPlatformComponent)
						movingPlatformComponent->StartPathing();					
				}
			}				

		} else {
			LOG_DEBUG("NjFireRocksManager.cpp a rock arrived, stopped pathing on that rock");
			// tell the platform to stop pathing
			auto* movingPlatformComponent = sender->GetComponent<MovingPlatformComponent>();
			if (!movingPlatformComponent) return;
			// soft stop
			movingPlatformComponent->StopPathing(false);
		}
	}
}