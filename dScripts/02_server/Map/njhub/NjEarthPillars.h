#pragma once
#include "CppScripts.h"

class NjEarthPillars : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	
	void OnFireEventServerSide(
		Entity* self,
		Entity* sender,
		std::string args,
		int32_t param1,
		int32_t param2,
		int32_t param3
	) override;
	
	void MovePillarUp(Entity* self);
	
	void OnTimerDone(Entity* self, std::string timerName) override;
};
