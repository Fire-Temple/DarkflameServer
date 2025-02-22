#pragma once
#include "CppScripts.h"

class BladesKillcountEvent : public CppScripts::Script
{
public:

	void OnStartup(Entity* self) override;

	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;

	void RunDieCallbacks(Entity* self);

	void EntitySmashed(Entity* self);

	void OnTimerDone(Entity* self, std::string timerName) override;

private:

    static Entity* player1;
    static Entity* player2;
    static Entity* player3;
    static Entity* player4;

};
