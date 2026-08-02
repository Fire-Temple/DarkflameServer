#pragma once
#include "CppScripts.h"

class NjLightningOrb : public CppScripts::Script
{
public:

	void OnCollisionPhantom(Entity* self, Entity* target) override;

	void ApplyEffect(Entity* self, Entity* target);
};