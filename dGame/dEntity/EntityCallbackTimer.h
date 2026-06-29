#pragma once

#include <string>
#include <functional>

class EntityCallbackTimer {
public:
	EntityCallbackTimer(const float time, const std::function<void()> callback, uint32_t id = 0);
	
	std::function<void()> GetCallback() const { return m_Callback; };

	float GetTime() const { return m_Time; };
	
	uint32_t GetID() const {return m_ID; };

	void Update(float deltaTime);

private:
	std::function<void()> m_Callback;
	float m_Time;
	uint32_t m_ID;
};
