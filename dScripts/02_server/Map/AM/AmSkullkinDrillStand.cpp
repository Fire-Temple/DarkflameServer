#include "AmSkullkinDrillStand.h"
#include "GameMessages.h"
#include "dpEntity.h"
#include "Entity.h"
#include "RenderComponent.h"

#include <algorithm>

void AmSkullkinDrillStand::OnStartup(Entity* self) {
	self->SetVar(u"bActive", false);
	self->SetVar<std::vector<Entity*>>(u"players", {});

	self->SetProximityRadius(new dpEntity(self->GetObjectID(), { 6, 14, 6 }), "knockback");
}

void AmSkullkinDrillStand::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) {
	auto myPos = self->GetPosition();
	
	if (name == "knockback") {
		auto& players = self->GetVar<std::vector<Entity*>>(u"players");
		
		for (auto* player : players) {
			if (!player || player->GetIsDead()) continue;
			auto playerPos = player->GetPosition();
			
			NiPoint3 newVec = { (playerPos.x - myPos.x) * 4.5f, 15, (playerPos.z - myPos.z) * 4.5f };
			GameMessages::SendKnockback(player->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 0, newVec);
			GameMessages::SendPlayFXEffect(player->GetObjectID(), 1378, u"create", "pushBack");
			RenderComponent::PlayAnimation(player, u"knockback-recovery");
		}
	}
}

void AmSkullkinDrillStand::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (entering->IsPlayer() && name == "knockback") {

		auto players = self->GetVar<std::vector<Entity*>>(u"players");

		if (status == "ENTER") {
			if (std::find(players.begin(), players.end(), entering) == players.end()) {
				players.push_back(entering);
			}
			
		} else if (status == "LEAVE") {
			// Remove the player
			players.erase(
				std::remove(players.begin(), players.end(), entering),
				players.end()
			);
		}

		self->SetVar(u"players", players);
	}
}
