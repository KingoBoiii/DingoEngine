#pragma once
#include <cstdint>

namespace Dingo
{

	// All sim randomness goes through these integer-only generators: std::rand and the standard
	// distributions differ between platforms, and a valley and its weather must not.
	inline uint64_t SplitMix64(uint64_t& state)
	{
		uint64_t z = (state += 0x9E3779B97F4A7C15ull);
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
		return z ^ (z >> 31);
	}

	inline uint64_t HashMix(uint64_t a, uint64_t b)
	{
		uint64_t state = a ^ (b * 0xD6E8FEB86659FD93ull);
		SplitMix64(state);
		return SplitMix64(state);
	}

	class Rng
	{
	public:
		explicit Rng(uint64_t seed)
		{
			uint64_t state = seed;
			for (uint64_t& word : m_State)
				word = SplitMix64(state);
		}

		uint64_t NextU64()
		{
			const uint64_t result = Rotl(m_State[1] * 5, 7) * 9;
			const uint64_t t = m_State[1] << 17;
			m_State[2] ^= m_State[0];
			m_State[3] ^= m_State[1];
			m_State[1] ^= m_State[2];
			m_State[0] ^= m_State[3];
			m_State[2] ^= t;
			m_State[3] = Rotl(m_State[3], 45);
			return result;
		}

		uint32_t NextU32() { return static_cast<uint32_t>(NextU64() >> 32); }

		// Inclusive range.
		int NextInt(int lo, int hi)
		{
			const uint64_t span = static_cast<uint64_t>(hi - lo) + 1;
			return lo + static_cast<int>((static_cast<uint64_t>(NextU32()) * span) >> 32);
		}

		bool Chance(int permille) { return NextInt(0, 999) < permille; }

	private:
		static uint64_t Rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

		uint64_t m_State[4];
	};

}
