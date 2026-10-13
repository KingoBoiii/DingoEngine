#pragma once
#include <chrono>

namespace Dingo
{

	/// A stopwatch on std::chrono::high_resolution_clock, for measuring durations in your own code.
	class Timer
	{
	public:
		/// Starts timing now.
		Timer();

		/// Restarts the count from zero.
		void Reset();

		/// Seconds since construction or the last Reset.
		float Elapsed() const;
		/// Milliseconds since construction or the last Reset.
		float ElapsedMillis() const;
	private:
		std::chrono::time_point<std::chrono::high_resolution_clock> m_Start;
	};

}
