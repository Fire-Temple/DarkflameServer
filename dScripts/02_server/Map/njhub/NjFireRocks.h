#pragma once
#include "CppScripts.h"

class NjFireRocks : public CppScripts::Script
{
public:

	void OnStartup(Entity* self) override;
	
	void OnWaypointReached(Entity* self, uint32_t waypointIndex) override;

};
