#include "ZoneFvPropertyMed.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "dZoneManager.h"
#include "Character.h"
#include "DestroyableComponent.h"
#include "Entity.h"
#include "RenderComponent.h"
#include "PropertyManagementComponent.h"
#include "MissionComponent.h"
#include "eMissionTaskType.h"
#include "eMissionState.h"

#include "PossessorComponent.h"
#include "dZoneManager.h"

void ZoneFvPropertyMed::SetGameVariables(Entity* self) {
	self->SetVar<std::string>(ClaimMarkerGroup, "");
	self->SetVar<std::string>(PropertyPlaqueGroup, "PropertyPlaque");
	self->SetVar<std::string>(PropertyVendorGroup, "PropertyVendor");
	self->SetVar<std::string>(ImagOrbGroup, "Orb");

//	self->SetVar<std::string>(ClaimMarkerSpawner, "");
	self->SetVar<std::string>(ImageOrbSpawner, "Orb");
	self->SetVar<std::string>(PropObjsSpawner, "Prop_Safe");
	self->SetVar<std::vector<std::string>>(BehaviorObjsSpawner, {});

	self->SetVar<int>(u"numSmashed", 0);

	self->SetVar<int32_t>(defeatedProperyFlag, 32451);
	self->SetVar<int32_t>(placedModelFlag, 0);
	self->SetVar<uint32_t>(brickLinkMissionIDFlag, 0);
	self->SetVar<std::string>(passwordFlag, "s3kratK1ttN");
	self->SetVar<LOT>(orbIDFlag, 10226);
}

void ZoneFvPropertyMed::CheckForOwner(Entity* self) {
	auto Plaques = Game::entityManager->GetEntitiesInGroup("PropertyPlaque");
	for (auto* plaque : Plaques) {	
		if (!plaque) {
			self->AddTimer(RunPlayerLoadedAgainTimer, 0.5f);
			return;
		}
	}
	
	self->SetI64(PropertyOwnerVariable, GetOwner());
}

void ZoneFvPropertyMed::OnStartup(Entity* self) {
	SetGameVariables(self);
}

void ZoneFvPropertyMed::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1,
	int32_t param2, int32_t param3) {
	if (args == CheckForPropertyOwnerEvent) {
		sender->SetNetworkVar<std::string>(PropertyOwnerIDVariable, std::to_string(self->GetVar<LWOOBJID>(PropertyOwnerVariable)));
	}
}

void ZoneFvPropertyMed::OnPlayerLoaded(Entity* self, Entity* player) {
	player->GetCharacter()->SetTargetScene("MedProperty");	
	
	CheckForOwner(self);

	auto rented = false;
	auto propertyOwner = PropertyManagementComponent::Instance()->GetOwnerId();

	self->OnFireEventServerSide(self, CheckForPropertyOwnerEvent);

	if (propertyOwner > 0) {
		rented = true;
	}

	if (propertyOwner < 0) {
		propertyOwner = LWOOBJID_EMPTY;
	}

	self->SetNetworkVar(PropertyOwnerIDVariable, propertyOwner);

	if (rented) {
//		tell client script prop is cleared	
		self->SetNetworkVar<int>(u"cleared", 1);		
		
		auto plaques = Game::entityManager->GetEntitiesInGroup("PropertyVendor");
		for (auto* plaque : plaques) {
			Game::entityManager->DestructEntity(plaque);
		}

		const auto& mapID = Game::zoneManager->GetZone()->GetZoneID();

		if (propertyOwner > 0) {
			auto* missionComponent = player->GetComponent<MissionComponent>();

			if (missionComponent != nullptr) {
				missionComponent->Progress(
					eMissionTaskType::VISIT_PROPERTY,
					mapID.GetMapID(),
					PropertyManagementComponent::Instance()->GetId()
				);
			}
		}


		// activate property safe spawner network
		for (auto* safe : Game::zoneManager->GetSpawnersByName("Prop_Safe")) {
			if (safe) {
				safe->Activate();
			}
		}			
						

		// tell client script who owns the property
		self->SetNetworkVar<std::string>(RenterVariable, std::to_string(propertyOwner));

		if (player->GetObjectID() != propertyOwner)
			self->SetNetworkVar<int>(u"BorderVisible", 0);
			return;
			
//		remove guard
//		for (auto* guard : Game::zoneManager->GetSpawnersInGroup("Guard")) {	
//			guard->Deactivate();
//			guard->DestroyAllEntities();	
//			If guard hasn't spawned yet			
//			guard->AddEntitySpawnedCallback([self, this](Entity* entity) {		
//				entity->Smash();
//				entity->Kill();		
//			});				
//		}	
		
	} else {
		const auto defeatedFlag = player->GetCharacter()->GetPlayerFlag(self->GetVar<int32_t>(defeatedProperyFlag));

		self->SetNetworkVar(UnclaimedVariable, true);
		self->SetVar<LWOOBJID>(PlayerIDVariable, player->GetObjectID());

		self->SetNetworkVar<int>(u"BorderVisible", 0);		

		if (!defeatedFlag) {			
//			tell client script prop is not cleared	
			self->SetNetworkVar<int>(u"cleared", 0);	
			
			GameMessages::SendPlayCinematic(player->GetObjectID(), u"ShowOrb_0", player->GetSystemAddress());	

//			orb fx spawner		
			for (auto* orbfx : Game::zoneManager->GetSpawnersInGroup("OrbFX")) {	
				orbfx->Activate();			
			}		
		
//			orb spawner		
			for (auto* orbSpawner : Game::zoneManager->GetSpawnersByName("Orb")) {
				orbSpawner->Activate();						
			}	

//			spawn smashables
			for (auto* smashable : Game::zoneManager->GetSpawnersInGroup("PropertySmashables")) {		
				smashable->Activate();
			}			
//			spawn enemies
			for (auto* enemy : Game::zoneManager->GetSpawnersInGroup("PropEnemies")) {					
				enemy->Activate();
				enemy->AddEntitySpawnedCallback([self, this](Entity* entity) {
					if (entity->GetLOT() == 32218) {
						entity->AddDieCallback([self, this]() {
							const auto numSmashed = self->GetVar<int>(u"numSmashed");
							if (numSmashed <= 1) {
								self->SetVar<int>(u"numSmashed", numSmashed + 1);
							} else {
								
								self->AddTimer("SmashEnemies", 0.1f);					
								self->AddTimer("HorsemenSmashed", 2);	
								self->SetVar<int>(u"numSmashed", 0);	
							}
						});	
					}
				});
			}	
							
			
//		claim marker spawner		
//			for (auto* markerSpawner : Game::zoneManager->GetSpawnersByName("")) {
//				markerSpawner->DestroyAllEntities();
//			}			
	
		} else {
//			tell client script prop is cleared	
			self->SetNetworkVar<int>(u"cleared", 1);		
		}
		
//		guard spawner
		for (auto* guard : Game::zoneManager->GetSpawnersInGroup("Guard")) {		
			guard->Activate();				
		}			
	}
}

void ZoneFvPropertyMed::OnZonePropertyRented(Entity* self, Entity* player) {
	GameMessages::SendPlayCinematic(player->GetObjectID(), u"ShowProperty", player->GetSystemAddress());

	self->AddTimer(BoundsVisOnTimer, 0.1f);
	self->SetVar<LWOOBJID>(PropertyOwnerVariable, player->GetObjectID());

	auto plaques = Game::entityManager->GetEntitiesInGroup("PropertyVendor");
	for (auto* plaque : plaques) {
		Game::entityManager->DestructEntity(plaque);
	}

	auto brickLinkMissionID = self->GetVar<uint32_t>(brickLinkMissionIDFlag);
	if (brickLinkMissionID != 0) {
		auto missionComponent = player->GetComponent<MissionComponent>();
		if (missionComponent) missionComponent->CompleteMission(brickLinkMissionID, true);
	}


	for (auto* safe : Game::zoneManager->GetSpawnersByName("Prop_Safe")) {
		if (safe) {
			safe->Activate();
		}
	}
	
//	remove guard
	for (auto* guard : Game::zoneManager->GetSpawnersInGroup("Guard")) {		
		guard->Deactivate();
		guard->DestroyAllEntities();			
	}		
}

void ZoneFvPropertyMed::OnZonePropertyModelPlaced(Entity* self, Entity* player) {
	auto* character = player->GetCharacter();
	if (character == nullptr)
		return;

	auto flag = self->GetVar<int32_t>(placedModelFlag);
	if (flag)
		character->SetPlayerFlag(flag, true);
}

void ZoneFvPropertyMed::OnPlayerExit(Entity* self, Entity* player) {
	if (self->GetBoolean(UnclaimedVariable)) {
		if (player->GetObjectID() == self->GetVar<LWOOBJID>(PlayerIDVariable)) {
			// Destroy all spawners	
		}
	}
}

LWOOBJID ZoneFvPropertyMed::GetOwner() {
	auto* manager = PropertyManagementComponent::Instance();
	return manager == nullptr ? LWOOBJID_EMPTY : manager->GetOwnerId();
}

void ZoneFvPropertyMed::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, 
int32_t param1, int32_t param2) {
	
}	

void ZoneFvPropertyMed::OnTimerDone(Entity* self, std::string timerName) {
	

	if (timerName == RunPlayerLoadedAgainTimer) {
		CheckForOwner(self);
	} else if (timerName == BoundsVisOnTimer) {
		self->SetNetworkVar<int>(u"BorderVisible", 1);
		
		auto BorderEntities = Game::entityManager->GetEntitiesInGroup("PropertyBorder");
		for (auto* border : BorderEntities) {	
			if (border) {
				RenderComponent::PlayAnimation(border, u"BorderIn");
			}
		}	
	} else if (timerName == "SmashEnemies") {		
//		clear enemies
		for (auto* enemy : Game::zoneManager->GetSpawnersInGroup("PropEnemies")) {		
			enemy->Deactivate();
			enemy->DestroyAllEntities();			
		}		

//		clear any spawned ronins
		for (auto* ronin : Game::entityManager->GetEntitiesByLOT(7815)) {		
			// ronin->Smash(ronin->GetObjectID(), eKillType::SILENT);	
			ronin->Smash(ronin->GetObjectID());
		}	
		
	} else if (timerName == "RespawnQB") {
		for (auto* platform : Game::zoneManager->GetSpawnersInGroup("MovingPlatformQB")) {	
			platform->Activate();			
		}	
	} else if (timerName == "ResetOrbNetworkVar") {			
		self->SetNetworkVar<int>(u"orb_smashed", 0);		
	} else if (timerName == "HorsemenSmashed") {		
		const auto playerID = self->GetVar<LWOOBJID>(PlayerIDVariable);
		auto* player = Game::entityManager->GetEntity(playerID);
		if (player) {
			GameMessages::SendPlayCinematic(player->GetObjectID(), u"ShowQB", player->GetSystemAddress());	
		}				

//		spawn platform QB	
		self->AddTimer("RespawnQB", 1);		

//		orb diecallback
		for (auto* orb : Game::entityManager->GetEntitiesByLOT(10226)) {
			orb->AddCollisionPhantomCallback([self, this](Entity* other) {	
				if (other != nullptr && other->IsPlayer() && !self->GetVar<bool>(CollidedVariable)) {
					self->SetVar<bool>(CollidedVariable, true);	
					
					self->SetNetworkVar<int>(u"orb_smashed", 1);
					self->SetNetworkVar<int>(u"cleared", 1);
					
					self->AddTimer("ResetOrbNetworkVar", 3);
						
					self->AddTimer("SetDefeatedFlag", 0.1f);										
					
					const auto playerID = self->GetVar<LWOOBJID>(PlayerIDVariable);
					auto* player = Game::entityManager->GetEntity(playerID);
					if (player != nullptr) {				
						GameMessages::SendPlayCinematic(playerID, u"OrbSmashed", player->GetSystemAddress());	
					}	
//					clear orb					
					for (auto* entity : Game::entityManager->GetEntitiesInGroup("Orb")) {
						entity->Smash();
					}			
					for (auto* spawner : Game::zoneManager->GetSpawnersByName("Orb")) {
						if (!spawner) return;
						spawner->Deactivate();						
					}	
//					clear orb fx
					for (auto* entity : Game::entityManager->GetEntitiesInGroup("OrbFX")) {
						GameMessages::SendStopFXEffect(entity, true, "maelstromGenerator");	
					}		
					for (auto* orbfx : Game::zoneManager->GetSpawnersInGroup("OrbFX")) {	
						orbfx->Deactivate();	
						orbfx->DestroyAllEntities();							
					}					
//					clear smashables
					for (auto* smashable : Game::entityManager->GetEntitiesInGroup("PropertySmashables")) {		
						smashable->Smash(player->GetObjectID(), eKillType::SILENT);	
					}	
					for (auto* smashable : Game::zoneManager->GetSpawnersInGroup("PropertySmashables")) {		
						smashable->Deactivate();	
					}	
//					clear platform qb					
					for (auto* platform : Game::zoneManager->GetSpawnersInGroup("MovingPlatformQB")) {		
						platform->Deactivate();
						platform->DestroyAllEntities();	
					}	
//					clear enemies				
					self->AddTimer("SmashEnemies", 0.2f);										
				}	
			});	

		}	
	} else if (timerName == "SetDefeatedFlag") {	
		const auto playerID = self->GetVar<LWOOBJID>(PlayerIDVariable);
		auto* player = Game::entityManager->GetEntity(playerID);	
		auto* character = player->GetCharacter();
		if (character != nullptr) {
			character->SetPlayerFlag(self->GetVar<int32_t>(defeatedProperyFlag), true);
		}			
	}
}


