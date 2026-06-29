#include "AgBusDoor.h"
#include "Entity.h"
#include "GameMessages.h"
#include "ProximityMonitorComponent.h"
#include "MovingPlatformComponent.h"

void AgBusDoor::OnStartup(Entity* self) {
	auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();	
	if (movingPlatformComponent != nullptr) movingPlatformComponent->SetNoAutoStart(true);	
	
	m_Counter = 0;
	m_OuterCounter = 0;
	self->SetProximityRadius(75, "busDoor");
	self->SetProximityRadius(85, "busDoorOuter");	
}

void AgBusDoor::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name != "busDoor" && name != "busDoorOuter") return;

	// Make sure only humans are taken into account
	if (!entering->GetCharacter()) return;

	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();

	if (proximityMonitorComponent == nullptr) return;

	m_Counter = 0;
	m_OuterCounter = 0;

	for (const auto id : proximityMonitorComponent->GetProximityObjects("busDoor")) {
		const auto* const entity = Game::entityManager->GetEntity(id);
		if (entity != nullptr && entity->IsPlayer()) m_Counter++;
	}

	for (const auto id : proximityMonitorComponent->GetProximityObjects("busDoorOuter")) {
		const auto* const entity = Game::entityManager->GetEntity(id);
		if (entity != nullptr && entity->IsPlayer()) m_OuterCounter++;
	}

	if (status == "ENTER") {
		// move up when a player is inside both radii
		if (m_Counter > 0) {
			MoveDoor(self, true);
		}
	} else if (status == "LEAVE") {
		// move down when no players are inside either radii
		if (m_Counter <= 0) {
			MoveDoor(self, false);
		}
	}
}

void AgBusDoor::MoveDoor(Entity* self, bool bOpen) {
	auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();	
	if (bOpen) {
		movingPlatformComponent->GotoWaypoint(0);
	} else {
		movingPlatformComponent->GotoWaypoint(1);
	}

	// This is currently commented out because it might be the reason that people's audio is cutting out.
	GameMessages::SendPlayNDAudioEmitter(self, UNASSIGNED_SYSTEM_ADDRESS, "{9a24f1fa-3177-4745-a2df-fbd996d6e1e3}");
}

void AgBusDoor::OnWaypointReached(Entity* self, uint32_t waypointIndex) {
	if (waypointIndex == 1) {
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 642, u"create", "busDust", LWOOBJID_EMPTY, 1.0f, 1.0f, true);
	}
}
