#pragma once
#include "CppScripts.h"

class FtWavesManager : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnCollisionPhantom(Entity* self, Entity* target) override;

	void ActivateWaveSpinners(Entity* self, Entity* target);

	void HandleSpinner(Entity* self, std::string spinner, std::string direction);

	void CheckWaveProgress(Entity* self);

	void OnTimerDone(Entity* self, std::string timerName) override;

private:


	int bStarted;
	int LargeGroup;
	int WaveNum;

	int Wave1Progress;
	int Wave2Progress;
	int Wave3Progress;


	int HandSpinnerUp;
	int BlacksmithSpinnerUp;
	int OverseerSpinnerUp;
	int MarksmanSpinnerUp;
	int WolfSpinnerUp;
	int BeetleSpinnerUp;
	int ScientistSpinnerUp;
	int BonezaiSpinnerUp;

};
