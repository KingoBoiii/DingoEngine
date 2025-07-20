#pragma once
#include <chrono>

namespace Dingo
{

	class Timer
	{
	public:
		Timer();

		void Reset();

		float Elapsed() const;
		float ElapsedMillis() const;
	private:
		std::chrono::time_point<std::chrono::high_resolution_clock> m_Start;
	};

}
