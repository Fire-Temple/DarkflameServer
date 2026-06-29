#pragma once
#include "CppScripts.h"

class FtZoneControl : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnZoneLoadedInfo(Entity* self);

	void OnPlayerLoaded(Entity* self, Entity* player) override;

	void OnPlayerExit(Entity* self, Entity* player) override;

private:

	void SetPlayerCounts(Entity* self);

	int PlayerCount;

	int BossPlayerCount;
	
	int ChestPlayerCount;

	int TeamChecked;

	int PlayersInTeam;


	int Player1Loaded = 0;
	int Player2Loaded = 0;
	int Player3Loaded = 0;
	int Player4Loaded = 0;

};
