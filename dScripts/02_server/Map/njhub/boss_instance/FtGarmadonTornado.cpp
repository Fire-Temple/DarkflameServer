// waypoints 0, 14, 24, 34, 40, 49, 56, 64, 72, 79

#include "FtGarmadonTornado.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"
#include "ProximityMonitorComponent.h"
#include "SkillComponent.h"
#include "EntityInfo.h"
#include "RenderComponent.h"

Entity* FtGarmadonTornado::randomPlayer = nullptr;

void FtGarmadonTornado::OnStartup(Entity* self) {	
	auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
	if (movingPlatformComponent == nullptr) return;	
	
	const auto BossManager = Game::entityManager->GetEntitiesInGroup("BossManager");	
	for (auto* manager : BossManager) {	
		randomNum = manager->GetVar<int>(u"randNum");	
	}
	
	self->AddTimer("StartPathing", 25);
}


void FtGarmadonTornado::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, 
int32_t param1, int32_t param2) {
	
	
	
	if (name == "FindAPath") {
		self->AddTimer("BombPlayer", 28);	
		
	} else if (name == "StopPathing") {	
		auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();	
		
		self->CancelTimer("BombPlayer");
		

	} else if (name == "PlaySummonAnim") {
		RenderComponent::PlayAnimation(self, u"attack");	
		
	} else if (name == "PlayerSent") {		
		
		RenderComponent::PlayAnimation(self, u"attack");	

	}
}	

void FtGarmadonTornado::OnWaypointReached(Entity* self, uint32_t waypointIndex) {
	auto* movingPlatform = self->GetComponent<MovingPlatformComponent>();
	uint32_t nextWaypoint = waypointIndex + 1;

}
	
//	TODO assign phantom physics to tornado for client

void FtGarmadonTornado::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, 
	int32_t param1, int32_t param2, int32_t param3) {
		
	if (args == "PlayerInTornado") {		
		LOG_DEBUG("A player entered the tornado");
		
//		OnHit skill for player			
		auto* skillComponent = self->GetComponent<SkillComponent>();				
		skillComponent->CastSkill(this->TornadoHitSkill, sender->GetObjectID());
			
//		Knockback for player			
		auto dir = QuatUtils::Forward(sender->GetRotation());
		dir.y = 18;
		dir.x = -dir.x * 21;
		dir.z = -dir.z * 21;
		GameMessages::SendKnockback(sender->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 1000, dir);
	}
}


void FtGarmadonTornado::OnTimerDone(Entity* self, std::string timerName) {
	
	if (timerName == "BombPlayer") {	
	
//		Request random player from manager	
		const auto BossManager = Game::entityManager->GetEntitiesInGroup("BossManager");	
		for (auto* manager : BossManager) {	
			manager->NotifyObject(self, "RequestPlayer");
		}

		self->AddTimer("BombPlayer", 28);		

	}
	if (timerName == "StartPathing") {

		auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
		if (movingPlatformComponent != nullptr)
			movingPlatformComponent->StartPathing();


		// backup logic if needed

		int waypoints[] = {0, 14, 24, 34, 40, 49, 56, 64, 72, 79};
		int scaledNum = randomNum + (std::rand() % 5);		
		int waypoint = waypoints[scaledNum];

		// GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, false, waypoint, waypoint + 1);	
		// self->AddTimer("StartPathing", 270.5f);
	}	
}


