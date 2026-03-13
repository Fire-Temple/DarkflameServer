#include "NjFireRocksManager.h"
#include "MovingPlatformComponent.h"

#include <algorithm>

//	Because time based movement,
// 	We just need to StartPathing at the same time

void NjFireRocksManager::OnStartup(Entity* self) {

//	set defaults	
	if (self->GetVar<std::u16string>(u"RockGroup") != u"LavaRocks02") {
		self->SetVar<std::u16string>(u"RockGroup", u"FireTransRocks");
	}
	if (self->GetVar<int32_t>(u"NumberOfRocks") != 2) {
		self->SetVar<int32_t>(u"NumberOfRocks", 3);
	}
	
	self->AddTimer("check", 7);		
}

void NjFireRocksManager::CheckForRocks(Entity* self) {
	auto rockGroup = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"RockGroup"));
	auto platforms = 0;
	
	// count all the moving rocks
	const auto rocks = Game::entityManager->GetEntitiesInGroup(rockGroup);
	for (auto* rock : rocks) {
		platforms++;
	}

	// wait for all rocks to be ready
	if (platforms < self->GetVar<int32_t>(u"NumberOfRocks")) {
		LOG_DEBUG("NjFireRocksManager.cpp waiting for rocks, loaded platforms: %u, for rock group: %s", platforms, rockGroup);
		self->AddTimer("check", 1);
		return;
	}
	LOG_DEBUG("NjFireRocksManager.cpp found all %u platforms", platforms);
	
	self->AddTimer("startPlatforms", 7);
}

void NjFireRocksManager::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "check") {	
		CheckForRocks(self);
		
	} else if (timerName == "startPlatforms") {
		auto rockGroup = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"RockGroup"));
		const auto rocks = Game::entityManager->GetEntitiesInGroup(rockGroup);
		for (auto* rock : rocks) {
			auto* movingPlatformComponent = rock->GetComponent<MovingPlatformComponent>();
			movingPlatformComponent->StartPathing();
		}
		LOG_DEBUG("NjFireRocksManager.cpp started the rock platforms");	
	}
}