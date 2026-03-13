#include "NjFireRocks.h"
#include "ProximityMonitorComponent.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "MovingPlatformComponent.h"
#include "EntityInfo.h"
#include "GameMessages.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"

//	Do nothing cause 1.11 has these set to timeBasedBovement

void NjFireRocks::OnStartup(Entity* self) {			
	// set defaults
	if (self->GetVar<std::u16string>(u"RockManagerGroup") != u"LavaRocks02Controller") {
		self->SetVar<std::u16string>(u"RockManagerGroup", u"FireTransRocksManager");
	}	
}
