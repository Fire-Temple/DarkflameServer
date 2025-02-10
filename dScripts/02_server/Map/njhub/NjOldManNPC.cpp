#include "NjOldManNPC.h"
#include "MissionComponent.h"
#include "Character.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "eMissionState.h"
#include "ePlayerFlag.h"

void NjOldManNPC::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {

	if (missionID == 1801 || missionID == 2039) {
		auto* character = target->GetCharacter();
		if (missionState == eMissionState::READY_TO_COMPLETE && character) {
		
			character->SetPlayerFlag(2021, false);
			character->SetPlayerFlag(2022, false);
			character->SetPlayerFlag(2023, false);
			character->SetPlayerFlag(2024, false);
			character->SetPlayerFlag(2025, false);
			character->SetPlayerFlag(2026, false);
		}	
	}	
}
