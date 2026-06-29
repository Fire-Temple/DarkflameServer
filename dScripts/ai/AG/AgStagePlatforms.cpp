#include "AgStagePlatforms.h"
#include "MovingPlatformComponent.h"

void AgStagePlatforms::OnStartup(Entity* self) {
	auto* component = self->GetComponent<MovingPlatformComponent>();
	if (component) component->SetNoAutoStart(true);
}
