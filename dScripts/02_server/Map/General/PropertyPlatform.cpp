#include "PropertyPlatform.h"
#include "QuickBuildComponent.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"

void PropertyPlatform::OnStartup(Entity* self) {
	auto* movingPlatform = self->GetComponent<MovingPlatformComponent>();
	if (movingPlatform == nullptr) return;
	
	// some of these have startPathingOnLoad, let's fix that
	movingPlatform->SetNoAutoStart(true);	
}

void PropertyPlatform::OnUse(Entity* self, Entity* user) {
	auto* quickBuildComponent = self->GetComponent<QuickBuildComponent>();
	
	if (quickBuildComponent != nullptr && quickBuildComponent->GetState() == eQuickBuildState::COMPLETED) {
		auto* movingPlatform = self->GetComponent<MovingPlatformComponent>();
		
		if (movingPlatform != nullptr)
			movingPlatform->GotoWaypoint(1);

		self->AddCallbackTimer(movementDelay + effectDelay, [self, this]() {
			self->SetNetworkVar<float_t>(u"startEffect", dieDelay);
			self->AddCallbackTimer(dieDelay, [self]() {
				self->Smash();
				});
			});
	}
}
