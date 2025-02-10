#pragma once
#include "CppScripts.h"

class NjOldManNPC : public CppScripts::Script
{

	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) override;
};