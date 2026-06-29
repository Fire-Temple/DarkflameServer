#include "SimpleMoverSwitch.h"
#include "MovingPlatformComponent.h"
#include "GameMessages.h"

//	ensure switch_reset_time >= platform move time in hf

void SimpleMoverSwitch::OnStartup(Entity* self) {
	
//	use switch_reset_time from button	
	const auto SwitchEntity = Game::entityManager->GetEntitiesInGroup(self->GetVarAsString(u"switchGroup"));
	for (auto* button : SwitchEntity) {		
		self->SetVar<int32_t>(u"switch_reset_time", button->GetVar<int32_t>(u"switch_reset_time"));	
	}

	self->SetVar<bool>(u"ReadyForUse", true);
}	

void SimpleMoverSwitch::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, 
	int32_t param1, int32_t param2, int32_t param3) {
	if (!self->GetVar<bool>(u"ReadyForUse")) return;
	
	if (args == "OnActivated") {	
			auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
			auto SwitchResetTime = self->GetVar<int32_t>(u"switch_reset_time");
			
			self->SetVar<bool>(u"ReadyForUse", false);
			if (movingPlatformComponent == nullptr) return;	
			
			movingPlatformComponent->GotoWaypoint(1);

			self->AddTimer("Reset", SwitchResetTime);
	}	
}

void SimpleMoverSwitch::OnTimerDone(Entity* self, std::string timerName) {
	
	if (timerName == "Reset") {				
		auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
		if (movingPlatformComponent == nullptr) return;	
		movingPlatformComponent->GotoWaypoint(0);

		self->SetVar<bool>(u"ReadyForUse", true);
	}
}	
