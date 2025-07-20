#include "depch.h"
#include "DingoEngine/Core/Timer.h"

namespace Dingo
{

	Timer::Timer()
	{
		Reset();
	}

	void Timer::Reset()
	{
		m_Start = std::chrono::high_resolution_clock::now();
	}

	float Timer::Elapsed() const
	{
		return std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(std::chrono::high_resolution_clock::now() - m_Start).count() / 1000.0f;
	}

	float Timer::ElapsedMillis() const
	{
		return Elapsed() * 1000.0f;
	}

}


