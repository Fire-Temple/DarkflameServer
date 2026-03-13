#include "AmSkullkinDrill.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"
#include "DestroyableComponent.h"
#include "ProximityMonitorComponent.h"
#include "MissionComponent.h"
#include "EntityInfo.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"

void AmSkullkinDrill::OnStartup(Entity* self) {
	self->SetNetworkVar(u"bIsInUse", false);
	self->SetVar(u"bActive", true);

	GameMessages::SendPlayFXEffect(self->GetObjectID(), -1, u"spin", "active");

	auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();

	if (movingPlatformComponent == nullptr) {
		return;
	}

	movingPlatformComponent->SetNoAutoStart(true);

	self->SetProximityRadius(5, "spin_distance");

	// tell the stand to knockback players
	auto* standObj = GetStandObj(self);			
	standObj->NotifyObject(self, "knockback");	
}

void AmSkullkinDrill::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {
	if (message != "NinjagoSpinEvent" || self->GetNetworkVar<bool>(u"bIsInUse")) {
		return;
	}

	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();
	if (proximityMonitorComponent == nullptr || !proximityMonitorComponent->IsInProximity("spin_distance", caster->GetObjectID())) {
		return;
	}

	self->SetVar(u"activaterID", caster->GetObjectID());

	self->SetNetworkVar(u"bIsInUse", true);

	TriggerDrill(self, caster);
}

void AmSkullkinDrill::OnUse(Entity* self, Entity* user) {
	if (self->GetNetworkVar<bool>(u"bIsInUse")) {
		return;
	}
	const auto userID = user->GetObjectID();
	
	// freeze the user
	GameMessages::SendSetStunned(userID, eStateChangeType::PUSH, user->GetSystemAddress(), LWOOBJID_EMPTY,
		true, true, true, true, true, true, true, true, true
	);

	self->SetNetworkVar(u"bIsInUse", true);

	GameMessages::SendPlayFXEffect(user->GetObjectID(), 5499, u"on-anim", "tornado");
	GameMessages::SendPlayFXEffect(user->GetObjectID(), 5502, u"on-anim", "staff");

	self->SetVar(u"userID", userID);
	self->SetVar(u"activaterID", userID);

	RenderComponent::PlayAnimation(user, "spinjitzu-staff-windup");
	self->AddTimer("windupDone", 1.0f);
}

void AmSkullkinDrill::TriggerDrill(Entity* self, Entity* caster) {
	
	const auto& cine = self->GetVar<std::u16string>(u"cinematic");
	if (!cine.empty()) {
		GameMessages::SendPlayCinematic(caster->GetObjectID(), cine, caster->GetSystemAddress());		
	}
	
	RenderComponent::PlayAnimation(self, u"slowdown");

	self->AddTimer("resetDrill", 20);

	auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
	if (movingPlatformComponent != nullptr) {
		movingPlatformComponent->GotoWaypoint(1);
	}
}

void AmSkullkinDrill::OnWaypointReached(Entity* self, uint32_t waypointIndex) {
	LOG_DEBUG("Skullkin Drill reached waypoint %u", waypointIndex);
	
	if (waypointIndex == 1) {
		SpawnBit(self);

		RenderComponent::PlayAnimation(self, u"no-spin");
		GameMessages::SendStopFXEffect(self, true, "active");
		GameMessages::SendPlayFXEffect(self->GetObjectID(), -1, u"indicator", "indicator");

		self->SetVar(u"bActive", false);

		const auto playerID = self->GetVar<LWOOBJID>(u"userID");

		auto* player = Game::entityManager->GetEntity(playerID);

		if (player != nullptr) {
			RenderComponent::PlayAnimation(player, "spinjitzu-staff-end");
			self->AddTimer("animDone", 0.5f);		
		}		
	} else {
		RenderComponent::PlayAnimation(self, u"idle");
		GameMessages::SendPlayFXEffect(self->GetObjectID(), -1, u"spin", "active");
		GameMessages::SendStopFXEffect(self, true, "indicator");	
	}
}

void AmSkullkinDrill::SpawnBit(Entity* self) {
	auto myPos = self->GetPosition();
	auto myRot = self->GetRotation();

	myPos.y -= 20.5f;

	EntityInfo info = {};
	info.lot = 12346;
	info.pos = myPos;
	info.rot = myRot;
	info.scale = 3; // Needs the scale, otherwise attacks fail
	info.spawnerID = self->GetObjectID();

	auto* child = Game::entityManager->CreateEntity(info);

	Game::entityManager->ConstructEntity(child);

	self->SetVar(u"ChildSmash", child->GetObjectID());

	child->AddDieCallback([this, self]() {
		const auto& userID = self->GetVar<LWOOBJID>(u"activaterID");
		auto* player = Game::entityManager->GetEntity(userID);
		auto* standObj = GetStandObj(self);
		
		// tell the stand to knockback players
		standObj->NotifyObject(self, "knockback");		
		
		if (self->GetVar<bool>(u"resetState")) return;
		self->CancelAllTimers();
		
		if (player != nullptr) {
			// Progress missions
			auto* missionComponent = player->GetComponent<MissionComponent>();

			if (missionComponent != nullptr) {
				for (const auto missionID : m_MissionsToUpdate) {
					missionComponent->ForceProgressValue(missionID, 1, self->GetLOT());
				}
			}
			self->Smash(player->GetObjectID(), eKillType::SILENT);
		} else {
			self->Smash(self->GetObjectID(), eKillType::SILENT);
		}

		if (standObj != nullptr) {
			GameMessages::SendPlayFXEffect(standObj->GetObjectID(), 4946, u"explode", "explode");
		}
	});	
}

void AmSkullkinDrill::OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) {
	
	if (!attacker->IsPlayer() || self->GetVar<bool>(u"bActive")) return;
	
	const auto childID = self->GetVar<LWOOBJID>(u"ChildSmash");
	auto* child = Game::entityManager->GetEntity(childID);
	
	// we can just smash the bit, let the die callback handle it
	if (child != nullptr) {
		child->Smash(self->GetObjectID(), eKillType::VIOLENT);
	}	
	
}

void AmSkullkinDrill::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "resetDrill") {
		const auto childID = self->GetVar<LWOOBJID>(u"ChildSmash");
		auto* child = Game::entityManager->GetEntity(childID);
		auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
	
		self->SetVar<bool>(u"resetState", true);

		if (child != nullptr) {
			child->Smash(self->GetObjectID(), eKillType::SILENT);
		}

		self->SetNetworkVar(u"bIsInUse", false);
		self->SetVar(u"bActive", true);
		self->SetVar(u"userID", LWOOBJID_EMPTY);
		self->SetVar(u"activaterID", LWOOBJID_EMPTY);

		if (movingPlatformComponent != nullptr) {
			movingPlatformComponent->GotoWaypoint(0);
		}
		
		self->SetVar<bool>(u"resetState", false);
	
	} else if (timerName == "windupDone") {
		auto playerID = self->GetVar<LWOOBJID>(u"userID");
		auto* player = Game::entityManager->GetEntity(playerID);

		TriggerDrill(self, player);
		
		RenderComponent::PlayAnimation(player, u"spinjitzu-staff-loop");
		
	} else if (timerName == "animDone") {
		auto playerID = self->GetVar<LWOOBJID>(u"userID");
		auto* player = Game::entityManager->GetEntity(playerID);	
		
		GameMessages::SendStopFXEffect(player, true, "tornado");
		GameMessages::SendStopFXEffect(player, true, "staff");
	
		// unfreeze the user
		GameMessages::SendSetStunned(playerID, eStateChangeType::POP, player->GetSystemAddress(), LWOOBJID_EMPTY,
			true, true, true, true, true, true, true, true, true
		);

		self->SetVar(u"userID", LWOOBJID_EMPTY);		
	}
}


Entity* AmSkullkinDrill::GetStandObj(Entity* self) {
	const auto& myGroup = self->GetGroups();

	if (myGroup.empty()) {
		return nullptr;
	}

	std::string groupName = "Drill_Stand_";

	groupName.push_back(myGroup[0][myGroup[0].size() - 1]);

	const auto standObjs = Game::entityManager->GetEntitiesInGroup(groupName);

	if (standObjs.empty()) {
		return nullptr;
	} else {	
		return standObjs[0];
	}
}
