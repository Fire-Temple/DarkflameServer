#pragma once
#include "CppScripts.h"

class AmSkullkinDrill : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) override;
	
	void OnUse(Entity* self, Entity* user) override;

	void TriggerDrill(Entity* self, Entity* caster);

	void OnWaypointReached(Entity* self, uint32_t waypointIndex) override;
	
	void SpawnBit(Entity* self);
	
	void OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) override;

	void OnTimerDone(Entity* self, std::string timerName) override;
	
	Entity* GetStandObj(Entity* self);

private:
	std::vector<int32_t> m_MissionsToUpdate = { 972, 1305, 1308 };
};
