#include "NjLightningOrb.h"

#include "GameMessages.h"

//	Let the client handle collision

void NjLightningOrb::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, 
	int32_t param1, int32_t param2, int32_t param3) {

	LOG("received fire event!");
	if (args == "PlayerHitOrb" && sender->IsPlayer()) {
		LOG("received fire event from a player!");		
		auto dir = QuatUtils::Forward(sender->GetRotation());

		dir.y = 15;
		dir.x = -dir.x * 2.5;
		dir.z = -dir.z * 2.5;
		GameMessages::SendKnockback(sender->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 1000, dir);
		
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 9326, u"knockback", "knockback");
		GameMessages::SendPlayFXEffect(sender->GetObjectID(), 6026, u"shock", "shock");
	}
}