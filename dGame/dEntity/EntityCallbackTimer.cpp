#include "EntityCallbackTimer.h"

EntityCallbackTimer::EntityCallbackTimer(const float time, const std::function<void()> callback, uint32_t id) {
	m_Time = time;
	m_Callback = callback;
	m_ID = id;
}

void EntityCallbackTimer::Update(float deltaTime) {
	m_Time -= deltaTime;
}
