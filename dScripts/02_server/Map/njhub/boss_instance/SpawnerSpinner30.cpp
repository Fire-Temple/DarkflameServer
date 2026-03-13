#include "SpawnerSpinner30.h"
#include "ProximityMonitorComponent.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "MovingPlatformComponent.h"
#include "EntityInfo.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"

void SpawnerSpinner30::OnStartup(Entity* self) {
	RenderComponent::PlayAnimation(self, u"idle");
}

void SpawnerSpinner30::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	
	statue = sender;
	const auto spawnNw = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"spawnNw1"));	
	
	if (args == "spinneron") {	
	

//The spawninFX is delayed by ~3 seconds, assuming it's intentional & timing everything around it:				
	
//		Spawner jumble
		if (spawnNw == "4StatuesBeetlesA") {
			if (!AIsActivated) {
				self->SetNetworkVar(u"enemiesalive", 2);				
				self->AddTimer("BeetlesA", 3.1f);
				self->AddTimer("MoveUp", 0.8f);	
				GameMessages::SendPlayFXEffect(self->GetObjectID(), 10102, u"create", "create");					
			}
		} else if (spawnNw == "4StatuesBeetlesB") {
			if (!BIsActivated) {
				self->SetNetworkVar(u"enemiesalive", 2);				
				self->AddTimer("BeetlesB", 3.1f);
				self->AddTimer("MoveUp", 0.8f);	
				GameMessages::SendPlayFXEffect(self->GetObjectID(), 10102, u"create", "create");	
			}
		} else if (spawnNw == "4StatuesBeetlesC") {
			if (!CIsActivated) {	
				self->SetNetworkVar(u"enemiesalive", 2);					
				self->AddTimer("BeetlesC", 3.1f);
				self->AddTimer("MoveUp", 0.8f);	
				GameMessages::SendPlayFXEffect(self->GetObjectID(), 10102, u"create", "create");	
			}
		} else if (spawnNw == "4StatuesBeetlesD") {
			if (!DIsActivated) {	
				self->SetNetworkVar(u"enemiesalive", 2);
				self->AddTimer("BeetlesD", 3.1f);
				self->AddTimer("MoveUp", 0.8f);	
				GameMessages::SendPlayFXEffect(self->GetObjectID(), 10102, u"create", "create");	
			}
		}

	
	}	
}

void SpawnerSpinner30::SpawnBeetles(Entity* self, const std::string& loc) {
	auto pos = self->GetPosition();
	auto rot = self->GetRotation();
	pos.y += self->GetVarAs<float>(u"vert_offset");

	auto newRot = rot;
	auto offset = self->GetVarAs<float>(u"hort_offset");

	std::vector<LDFBaseData*> config = { new LDFData<std::string>(u"Leg", loc) };

	EntityInfo info{};
	info.lot = 13997;
	info.spawnerID = self->GetObjectID();
	info.settings = config;
	info.rot = newRot;

	if (loc == "BeetlesA1") {
		const auto dir = QuatUtils::Right(rot);	
		pos.x = -678.97211;
		pos.y = 263.9136;
		pos.z = -485.1644;
		info.pos = pos;
	} else if (loc == "BeetlesA2") {		
		const auto dir = QuatUtils::Right(rot);			
		pos.x = -693.16827;
		pos.y = 263.91735;
		pos.z = -485.13586;
		info.pos = pos;
	} else if (loc == "BeetlesB1") {
		const auto dir = QuatUtils::Right(rot);		
		pos.x = -489.10843;
		pos.y = 264.16498;
		pos.z = -483.99808;
		info.pos = pos;
	} else if (loc == "BeetlesB2") {		
		const auto dir = QuatUtils::Right(rot);			
		pos.x = -503.08203;
		pos.y = 264.02432;
		pos.z = -483.75735;
		info.pos = pos;
	} else if (loc == "BeetlesC1") {
		const auto dir = QuatUtils::Right(rot);			
		pos.x = -496.74146;
		pos.y = 263.9549;
		pos.z = -628.29474;
		info.pos = pos;
	} else if (loc == "BeetlesC2") {		
		const auto dir = QuatUtils::Right(rot);		
		pos.x = -481.57059;
		pos.y = 264.1012;
		pos.z = -627.54999;
		info.pos = pos;
	} else if (loc == "BeetlesD1") {
		const auto dir = QuatUtils::Right(rot);		
		pos.x = -702.24298;
		pos.y = 263.92444;
		pos.z = -627.49658;
		info.pos = pos;
	} else if (loc == "BeetlesD2") {		
		const auto dir = QuatUtils::Right(rot);		
		pos.x = -687.06604;
		pos.y = 263.91791;
		pos.z = -627.74445;
		info.pos = pos;
	}

	info.rot = QuatUtils::LookAt(info.pos, statue->GetPosition());

	auto* entity = Game::entityManager->CreateEntity(info);

	Game::entityManager->ConstructEntity(entity);

	OnChildLoaded(self, entity);
}

void SpawnerSpinner30::OnChildLoaded(Entity* self, Entity* child) {	
	const auto selfID = self->GetObjectID();	
	child->AddDieCallback([this, selfID, child]() {
		auto* self = Game::entityManager->GetEntity(selfID);
		auto* destroyableComponent = child->GetComponent<DestroyableComponent>();

		if (destroyableComponent == nullptr || self == nullptr) {
			return;
		}

// Get the current value of the "enemiesalive" variable

		auto EnemiesAlive = self->GetNetworkVar<int>(u"enemiesalive");
		// Decrement the value by 1
		if (EnemiesAlive > 0) {
			EnemiesAlive--;
			self->SetNetworkVar(u"enemiesalive", EnemiesAlive);
		}



		});
}

void SpawnerSpinner30::OnTimerDone(Entity* self, std::string timerName) {
	
	if (timerName == "MoveUp") {	
		auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
		auto* subComponent = movingPlatformComponent->GetMoverSubComponent();
		LOG("CURRENT waypoint %u next waypoint %u", subComponent->mCurrentWaypointIndex, subComponent->mNextWaypointIndex);
		
		movingPlatformComponent->GotoWaypoint(1);
		RenderComponent::PlayAnimation(self, u"up");	
		
//		Ascend sfx
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{7f770ade-b84c-46ad-b3ae-bdbace5985d4}");	
	
	} else if (timerName == "BeetlesA") {
		AIsActivated = true;
		SpawnBeetles(self, "BeetlesA1");
		SpawnBeetles(self, "BeetlesA2");
		
	} else if (timerName == "BeetlesB") {	
		BIsActivated = true;
		SpawnBeetles(self, "BeetlesB1");	
		SpawnBeetles(self, "BeetlesB2");
		
	} else if (timerName == "BeetlesC") {	
		CIsActivated = true;
		SpawnBeetles(self, "BeetlesC1");	
		SpawnBeetles(self, "BeetlesC2");
		
	} else if (timerName == "BeetlesD") {		
		DIsActivated = true;
		SpawnBeetles(self, "BeetlesD1");	
		SpawnBeetles(self, "BeetlesD2");	
	}
}

//Statue default val
Entity* SpawnerSpinner30::statue = nullptr;
