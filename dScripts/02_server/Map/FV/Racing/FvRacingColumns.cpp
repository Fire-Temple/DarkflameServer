#include "FvRacingColumns.h"
#include "MovingPlatformComponent.h"

void FvRacingColumns::OnStartup(Entity* self) {
	auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
	if (!movingPlatformComponent) return;
	movingPlatformComponent->SetNoAutoStart(true);
	
	// attached_path_start handled in MovingPlatformComponent
}
