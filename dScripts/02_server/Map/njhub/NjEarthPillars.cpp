#include "NjEarthPillars.h"

#include "MovingPlatformComponent.h"

//	Should work now :)

void NjEarthPillars::OnStartup(Entity* self) {
	self->SetVar<std::vector<Entity*>>(u"players", {});
}

void NjEarthPillars::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, 
	int32_t param1, int32_t param2, int32_t param3) {

	auto players = self->GetVar<std::vector<Entity*>>(u"players");
	auto moving = self->GetVar<bool>(u"Moving");
	
	if (sender == nullptr) return;
	
	if (args == "PlayerOnPillar") {
		auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
		if (movingPlatformComponent == nullptr) return;	
		
        if (std::find(players.begin(), players.end(), sender) != players.end()) {
            return; // sender exists
        }	

        // else, add sender
        players.push_back(sender);
        self->SetVar(u"players", players);

		if (!moving && !players.empty()) {
			movingPlatformComponent->GotoWaypoint(1);
			self->SetVar<bool>(u"Moving", true);
			self->AddTimer("CheckOnPlayers", 5);
		}
		
	} else if (args == "PlayerOffPillar") {
		
		if (moving && !players.empty()) {
			// find player
			auto player = std::find(players.begin(), players.end(), sender);
			if (player != players.end()) {
				// remove them
				players.erase(player);
				self->SetVar(u"players", players);


				if (!players.empty()) return;
			}
		}	
		
		MovePillarUp(self);
	}
}

void NjEarthPillars::MovePillarUp(Entity* self) {
	auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
	if (movingPlatformComponent == nullptr) return;		
	
	movingPlatformComponent->GotoWaypoint(0);
	
	self->SetVar<bool>(u"Moving", false);
	self->CancelTimer("CheckOnPlayers");
}

void NjEarthPillars::OnTimerDone(Entity* self, std::string timerName) {
    if (timerName == "CheckOnPlayers") {
        auto players = self->GetVar<std::vector<Entity*>>(u"players");
        auto moving = self->GetVar<bool>(u"Moving");

        if (moving && !players.empty()) {
            // TODO cleanup invalid players
            players.erase(
                std::remove(players.begin(), players.end(), nullptr),
                players.end()
            );
			
			self->SetVar(u"players", players);

            if (players.empty()) {
				MovePillarUp(self);
            } else {
                // keep checking
                self->AddTimer("CheckOnPlayers", 5.0f);
            }
        }
    }
}