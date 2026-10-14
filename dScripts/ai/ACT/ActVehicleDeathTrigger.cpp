#include "ActVehicleDeathTrigger.h"
#include "GameMessages.h"
#include "RacingControlComponent.h"
#include "dZoneManager.h"
#include "PossessorComponent.h"

void ActVehicleDeathTrigger::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target->IsPlayer()) return;

	auto* possessorComponent = target->GetComponent<PossessorComponent>();
	if (possessorComponent != nullptr) {
		auto* vehicle = Game::entityManager->GetEntity(possessorComponent->GetPossessable());
		if (vehicle != nullptr) {
			GameMessages::SendDie(vehicle, self->GetObjectID(), LWOOBJID_EMPTY, true, eKillType::VIOLENT, u"", 0, 0, 0, true, false, 0);

			auto* zoneController = Game::zoneManager->GetZoneControlObject();
			auto* racingControlComponent = zoneController->GetComponent<RacingControlComponent>();
			if (racingControlComponent != nullptr) {
				racingControlComponent->OnRequestDie(target);
			}	
		}	
	}
}
