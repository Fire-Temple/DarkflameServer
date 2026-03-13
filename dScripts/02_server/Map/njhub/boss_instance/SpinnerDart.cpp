#include "SpinnerDart.h"
#include "ProximityMonitorComponent.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "MovingPlatformComponent.h"
#include "EntityInfo.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"
#include "SkillComponent.h"

void SpinnerDart::OnStartup(Entity* self) {
	self->SetNetworkVar(u"bIsInUse", false);
	self->SetVar(u"bActive", true);
	
	if (self->GetVar<bool>(u"platformStartAtEnd")) {
		self->SetVar<int>(u"IsUp", 1);
		self->AddTimer("Shoot", 1);
	}
}

void SpinnerDart::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1,
	int32_t param2) {
	
	if (name == "DartSpin") {
		if (self->GetVar<int>(u"IsUp") == 1) {			
			self->AddTimer("MoveDown", 0.1f);			
		} else if (self->GetVar<int>(u"IsUp") == 0) {		
			self->AddTimer("MoveUp", 0.1f);			
		}
	}
}		

void SpinnerDart::OnTimerDone(Entity* self, std::string timerName) {
	auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
	
	if (timerName == "MoveUp") {
		self->SetVar<int>(u"IsUp", 1);	
		
		RenderComponent::PlayAnimation(self, u"up");

		movingPlatformComponent->GotoWaypoint(1);

		self->AddTimer("IdleUp", 1);	
		
//		Ascend sfx
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{7f770ade-b84c-46ad-b3ae-bdbace5985d4}");
	}
	
	else if (timerName == "MoveDown") {	
		self->SetVar<int>(u"IsUp", 0);	
		self->CancelTimer("Shoot");	
	
//		const auto RailEntity = Game::entityManager->GetEntitiesInGroup("DartSpinnersFRail1");	
//		for (auto* rail : RailEntity) rail->SetVar<int>(u"SpinnerIsUp", 0);

		self->AddTimer("RailDown", 1.5f);		

		RenderComponent::PlayAnimation(self, u"down");

		movingPlatformComponent->GotoWaypoint(0);
		
		self->CancelTimer("IdleUp");		
		
//		Descend sfx
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{97b60c03-51f2-45b6-80cc-ccbbef0d94cf}");	
	}
	
	else if (timerName == "IdleUp") {	
		RenderComponent::PlayAnimation(self, u"idle-up");	
		if (self->GetVar<int>(u"IsUp") == 1) {
			self->AddTimer("Shoot", 1);		
		}	
	}
	else if (timerName == "Shoot") {	
		RenderComponent::PlayAnimation(self, u"attack");	
		
		auto* skillComponent = self->GetComponent<SkillComponent>();
		if (skillComponent == nullptr) {
			return;
		}
		skillComponent->CalculateBehavior(972, 20376, self->GetObjectID(), true);			

//		BehaviorTemplate: 20375 | 8 | 4648
//		To add annoying cast effect back if you wish 	
				
		
		if (self->GetVar<int>(u"IsUp") == 1) {
			
			if (self->GetVar<bool>(u"platformStartAtEnd")) {
				const auto RailEntity = Game::entityManager->GetEntitiesInGroup("DartSpinnersFRail1");	
				for (auto* rail : RailEntity) {	
					rail->SetVar<int>(u"SpinnerIsUp", 1);
				}				
			} else {
				const auto RailEntity = Game::entityManager->GetEntitiesInGroup("DartSpinnersFRail2");	
				for (auto* rail : RailEntity) {	
					rail->SetVar<int>(u"SpinnerIsUp", 1);
				}						
			}
			
			self->AddTimer("Shoot", 1);		
		}			
		
	}
	else if (timerName == "RailDown") {	
		if (self->GetVar<bool>(u"platformStartAtEnd")) {		
			const auto RailEntity = Game::entityManager->GetEntitiesInGroup("DartSpinnersFRail1");	
			for (auto* rail : RailEntity) {	
				rail->SetVar<int>(u"SpinnerIsUp", 0);
			}	
		} else {	
			const auto RailEntity = Game::entityManager->GetEntitiesInGroup("DartSpinnersFRail2");	
			for (auto* rail : RailEntity) {	
				rail->SetVar<int>(u"SpinnerIsUp", 0);
			}	
		}
	}	
}

