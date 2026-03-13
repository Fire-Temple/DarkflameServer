#pragma once
#include "CppScripts.h"

class NjFireRocksManager : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void CheckForRocks(Entity* self);
	
	void OnTimerDone(Entity* self, std::string timerName) override;
};