#include "NjPropertyServer.h"
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

#include "dZoneManager.h"

void NjPropertyServer::SetGameVariables(Entity* self) {
	self->SetVar<std::string>(ClaimMarkerGroup, "");
	self->SetVar<std::string>(PropertyPlaqueGroup, "PropertyPlaque");
	self->SetVar<std::string>(PropertyVendorGroup, "PropertyVendor");
	self->SetVar<std::string>(ImagOrbGroup, "Orb");

//	self->SetVar<std::string>(ClaimMarkerSpawner, "");
	self->SetVar<std::string>(ImageOrbSpawner, "Orb");
	self->SetVar<std::string>(PropObjsSpawner, "Prop_Safe");
	self->SetVar<std::vector<std::string>>(BehaviorObjsSpawner, {});

	self->SetVar<int32_t>(defeatedProperyFlag, Game::zoneManager->GetZoneID().GetMapID() + 30000);
	self->SetVar<int32_t>(placedModelFlag, 0);
	self->SetVar<uint32_t>(brickLinkMissionIDFlag, 0);
	self->SetVar<std::string>(passwordFlag, "s3kratK1ttN");
	self->SetVar<LOT>(orbIDFlag, 10226);
}

void NjPropertyServer::CheckForOwner(Entity* self) {
	auto Plaques = Game::entityManager->GetEntitiesInGroup("PropertyPlaque");
	for (auto* plaque : Plaques) {	
		if (!plaque) {
			self->AddTimer(RunPlayerLoadedAgainTimer, 0.5f);
			return;
		}
	}
	
	self->SetI64(PropertyOwnerVariable, GetOwner());
}

void NjPropertyServer::OnStartup(Entity* self) {
	SetGameVariables(self);
}

void NjPropertyServer::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1,
	int32_t param2, int32_t param3) {
	if (args == CheckForPropertyOwnerEvent) {
		sender->SetNetworkVar<std::string>(PropertyOwnerIDVariable, std::to_string(self->GetVar<LWOOBJID>(PropertyOwnerVariable)));
	}
}

void NjPropertyServer::OnPlayerLoaded(Entity* self, Entity* player) {
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
	} else {
		const auto defeatedFlag = player->GetCharacter()->GetPlayerFlag(self->GetVar<int32_t>(defeatedProperyFlag));

		self->SetNetworkVar(UnclaimedVariable, true);
		self->SetVar<LWOOBJID>(PlayerIDVariable, player->GetObjectID());

		self->SetNetworkVar<int>(u"BorderVisible", 0);

		if (!defeatedFlag) {
//			TODO: set network var for no base music	

			GameMessages::SendPlayCinematic(player->GetObjectID(), u"ShowOrb", player->GetSystemAddress());

//		orb spawner		
			for (auto* orbSpawner : Game::zoneManager->GetSpawnersByName("Orb")) {
				orbSpawner->Activate();
			}		
			
//		claim marker spawner		
//			for (auto* markerSpawner : Game::zoneManager->GetSpawnersByName("")) {
//				markerSpawner->DestroyAllEntities();
//			}			
		
			self->AddTimer(StartOrbTimer, 0.1f);		
		} else {
//			TODO: set network var for base music on					
		}
	}
}

void NjPropertyServer::OnZonePropertyRented(Entity* self, Entity* player) {
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
}

void NjPropertyServer::OnZonePropertyModelPlaced(Entity* self, Entity* player) {
	auto* character = player->GetCharacter();
	if (character == nullptr)
		return;

	auto flag = self->GetVar<int32_t>(placedModelFlag);
	if (flag)
		character->SetPlayerFlag(flag, true);
}

void NjPropertyServer::OnPlayerExit(Entity* self, Entity* player) {
	if (self->GetBoolean(UnclaimedVariable)) {
		if (player->GetObjectID() == self->GetVar<LWOOBJID>(PlayerIDVariable)) {
			// Destroy all spawners
		}
	}
}

LWOOBJID NjPropertyServer::GetOwner() {
	auto* manager = PropertyManagementComponent::Instance();
	return manager == nullptr ? LWOOBJID_EMPTY : manager->GetOwnerId();
}

void NjPropertyServer::OnTimerDone(Entity* self, std::string timerName) {
	
	if (timerName == StartOrbTimer) {
		self->SetVar<bool>(CollidedVariable, false);
		auto orbs = Game::entityManager->GetEntitiesInGroup(self->GetVar<std::string>(ImagOrbGroup));
		if (orbs.empty()) {
			self->AddTimer(StartOrbTimer, 0.5f);
			return;
		}

		for (auto* orb : orbs) {
			orb->AddCollisionPhantomCallback([self, this](Entity* other) {
				if (other != nullptr && other->IsPlayer() && !self->GetVar<bool>(CollidedVariable)) {
					self->SetVar<bool>(CollidedVariable, true);

//					GameMessages::SendPlayCinematic(other->GetObjectID(), u"DestroyMaelstrom", other->GetSystemAddress());

					// Notifies the client that the property has been claimed with a flag, completes missions too
					auto* player = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(PlayerIDVariable));
					if (player != nullptr) {
						auto* character = player->GetCharacter();
						if (character != nullptr) {
							character->SetPlayerFlag(self->GetVar<int32_t>(defeatedProperyFlag), true);
						}
					}	
					
					for (auto* entity : Game::entityManager->GetEntitiesInGroup("Orb")) {
						entity->Smash();
					}

					for (auto* spawner : Game::zoneManager->GetSpawnersByName("Orb")) {
						if (!spawner) return;
						spawner->DestroyAllEntities();
						spawner->Deactivate();
					}				
					
				}
				});
		}
	} else if (timerName == RunPlayerLoadedAgainTimer) {
		CheckForOwner(self);
	} else if (timerName == BoundsVisOnTimer) {
		self->SetNetworkVar<int>(u"BorderVisible", 1);
		
		auto BorderEntities = Game::entityManager->GetEntitiesInGroup("PropertyBorder");
		for (auto* border : BorderEntities) {	
			if (border) {
				RenderComponent::PlayAnimation(border, u"BorderIn");
			}
		}		
	}	
}


