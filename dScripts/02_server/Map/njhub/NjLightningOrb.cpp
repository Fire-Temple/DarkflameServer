#include "NjLightningOrb.h"

#include "GameMessages.h"

void NjLightningOrb::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target->IsPlayer()) return;

	ApplyEffect(self, target);
}

void NjLightningOrb::ApplyEffect(Entity* self, Entity* target) {
	LOG("applying orb shock effect to player!");		
	auto dir = QuatUtils::Forward(target->GetRotation());

	dir.y = 15;
	dir.x = -dir.x * 2.5;
	dir.z = -dir.z * 2.5;
	GameMessages::SendKnockback(target->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 1000, dir);

	GameMessages::SendPlayFXEffect(self->GetObjectID(), 9326, u"knockback", "knockback");
	GameMessages::SendPlayFXEffect(target->GetObjectID(), 6026, u"shock", "shock");
}