#include "FtFlameJetServer.h"
#include "DestroyableComponent.h"
#include "SkillComponent.h"
#include "GameMessages.h"
#include "EntityInfo.h"
#include "EntityManager.h"


void FtFlameJetServer::OnStartup(Entity* self) {
	self->SetNetworkVar<bool>(u"FlameOn", true);
	self->SetVar<uint8_t>(u"flameProgress", 0);
}

void FtFlameJetServer::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target->IsPlayer()) {
		return;
	}

	if (!self->GetNetworkVar<bool>(u"FlameOn")) {
		return;
	}

	auto* skillComponent = target->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) {
		return;
	}

	skillComponent->CalculateBehavior(726, 11723, target->GetObjectID(), true);

	auto dir = QuatUtils::Forward(target->GetRotation());
	dir.y = 25;
	dir.x = -dir.x * 15;
	dir.z = -dir.z * 15;

	GameMessages::SendKnockback(target->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 1000, dir);
}

void FtFlameJetServer::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1,
	int32_t param2) {	
	const auto blockingVolume = self->GetVar<std::u16string>(u"blockingVolume");
	auto deactivatedName = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"name_deactivated_event"));
	auto progress = self->GetVar<uint8_t>(u"flameProgress");
	
	if (name == deactivatedName) {
		self->SetVar<uint8_t>(u"flameProgress", progress + 1);
		if (blockingVolume != u"FlameJetBlocker01" || progress >= 2) {
			self->AddTimer("FlamesOff", 2);
		}
	}
}

void FtFlameJetServer::OnTimerDone(Entity* self, std::string timerName) {	
	if (timerName == "FlamesOff") {	
		self->SetNetworkVar<bool>(u"FlameOn", false);	
		self->AddTimer("SmashSelf", 2);	
	} else if (timerName == "SmashSelf") {	
		self->Smash(self->GetObjectID(), eKillType::SILENT);
	}
}



