//	For fire chest puzzle in 8spinners

#include "Button70.h"
#include "Entity.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"
#include "EntityInfo.h"

void Button70::OnStartup(Entity* self) {
	isInMovement = false;
}

void Button70::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	
	if (args == "OnActivated") {
		if (!isInMovement) {	
			auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();		
			movingPlatformComponent->GotoWaypoint(0);
			self->AddTimer("MoveBack", 5.0f);	
		}	

		isInMovement = true;
	} 
}

void Button70::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, 
int32_t param1, int32_t param2) {
	
	if (name == "OnChestActivated") {		
		auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
		movingPlatformComponent->GotoWaypoint(1);	
	}
}	

void Button70::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "MoveBack") {	
		auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();		
		movingPlatformComponent->GotoWaypoint(1);
		isInMovement = false;	
	}
}
