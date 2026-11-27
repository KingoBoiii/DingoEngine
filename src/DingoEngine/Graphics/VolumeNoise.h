#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// The FogVolume noise texture. Generated on the CPU so a test can rebuild the exact texels the shader samples.
namespace Dingo::Internal
{

	constexpr uint32_t k_VolumeNoiseSize = 32;

	namespace VolumeNoiseDetail
	{

		inline uint32_t Hash(uint32_t x, uint32_t y, uint32_t z, uint32_t seed)
		{
			uint32_t h = x * 374761393u + y * 668265263u + z * 2147483647u + seed * 2246822519u;
			h = (h ^ (h >> 13)) * 1274126177u;
			h = (h ^ (h >> 16)) * 2654435761u;
			return h ^ (h >> 15);
		}

		inline float Dot(uint32_t hash, float x, float y, float z)
		{
			static constexpr float k_Gradients[12][3] = {
				{ 1, 1, 0 }, { -1, 1, 0 }, { 1, -1, 0 }, { -1, -1, 0 }, { 1, 0, 1 }, { -1, 0, 1 },
				{ 1, 0, -1 }, { -1, 0, -1 }, { 0, 1, 1 }, { 0, -1, 1 }, { 0, 1, -1 }, { 0, -1, -1 } };
			const float* g = k_Gradients[hash % 12u];
			return g[0] * x + g[1] * y + g[2] * z;
		}

		inline float Fade(float t)
		{
			return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
		}

		// Gradient noise that repeats every `period` lattice cells, about -1..1.
		inline float Periodic(float x, float y, float z, uint32_t period, uint32_t seed)
		{
			const float fx = std::floor(x);
			const float fy = std::floor(y);
			const float fz = std::floor(z);
			const uint32_t x0 = static_cast<uint32_t>(static_cast<int32_t>(fx)) % period;
			const uint32_t y0 = static_cast<uint32_t>(static_cast<int32_t>(fy)) % period;
			const uint32_t z0 = static_cast<uint32_t>(static_cast<int32_t>(fz)) % period;
			const uint32_t x1 = (x0 + 1u) % period;
			const uint32_t y1 = (y0 + 1u) % period;
			const uint32_t z1 = (z0 + 1u) % period;
			const float dx = x - fx;
			const float dy = y - fy;
			const float dz = z - fz;
			const float u = Fade(dx);
			const float v = Fade(dy);
			const float w = Fade(dz);

			const auto corner = [&](uint32_t cx, uint32_t cy, uint32_t cz, float ox, float oy, float oz)
			{
				return Dot(Hash(cx, cy, cz, seed), ox, oy, oz);
			};
			const auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

			const float x00 = lerp(corner(x0, y0, z0, dx, dy, dz), corner(x1, y0, z0, dx - 1.0f, dy, dz), u);
			const float x10 = lerp(corner(x0, y1, z0, dx, dy - 1.0f, dz), corner(x1, y1, z0, dx - 1.0f, dy - 1.0f, dz), u);
			const float x01 = lerp(corner(x0, y0, z1, dx, dy, dz - 1.0f), corner(x1, y0, z1, dx - 1.0f, dy, dz - 1.0f), u);
			const float x11 = lerp(corner(x0, y1, z1, dx, dy - 1.0f, dz - 1.0f), corner(x1, y1, z1, dx - 1.0f, dy - 1.0f, dz - 1.0f), u);
			return lerp(lerp(x00, x10, v), lerp(x01, x11, v), w);
		}

	}

	// k_VolumeNoiseSize^3 R8 texels, x fastest, of two octaves of gradient noise (4 and 8 cells across the
	// texture) that tile in every axis: the texel past the last one is the first.
	inline std::vector<uint8_t> GenerateVolumeNoise()
	{
		constexpr uint32_t size = k_VolumeNoiseSize;
		std::vector<uint8_t> texels(static_cast<size_t>(size) * size * size);
		for (uint32_t z = 0; z < size; ++z)
		{
			for (uint32_t y = 0; y < size; ++y)
			{
				for (uint32_t x = 0; x < size; ++x)
				{
					const float px = static_cast<float>(x) / static_cast<float>(size);
					const float py = static_cast<float>(y) / static_cast<float>(size);
					const float pz = static_cast<float>(z) / static_cast<float>(size);
					const float low = VolumeNoiseDetail::Periodic(px * 4.0f, py * 4.0f, pz * 4.0f, 4u, 1u);
					const float high = VolumeNoiseDetail::Periodic(px * 8.0f, py * 8.0f, pz * 8.0f, 8u, 2u);
					const float value = std::clamp((low * 0.65f + high * 0.35f) * 1.1f, -1.0f, 1.0f);
					texels[(static_cast<size_t>(z) * size + y) * size + x] = static_cast<uint8_t>(std::lround((value * 0.5f + 0.5f) * 255.0f));
				}
			}
		}
		return texels;
	}

}
