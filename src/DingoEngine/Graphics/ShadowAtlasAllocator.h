#pragma once

// Engine-internal: square power-of-two tiles of a square shadow atlas: each takes the smallest free
// square that holds it, split in four until it fits. Renderer3D makes one a scene.

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Dingo::Internal
{

	class ShadowAtlasAllocator
	{
	public:
		explicit ShadowAtlasAllocator(uint32_t size) { m_Free.push_back({ 0, 0, size }); }

		bool Allocate(uint32_t size, glm::uvec2& corner)
		{
			int best = -1;
			for (int i = 0; i < static_cast<int>(m_Free.size()); ++i)
			{
				if (m_Free[i].Size >= size && (best < 0 || m_Free[i].Size < m_Free[best].Size))
					best = i;
			}
			if (best < 0)
				return false;

			Square square = m_Free[best];
			m_Free.erase(m_Free.begin() + best);
			while (square.Size > size)
			{
				const uint32_t half = square.Size / 2;
				m_Free.push_back({ square.X + half, square.Y, half });
				m_Free.push_back({ square.X, square.Y + half, half });
				m_Free.push_back({ square.X + half, square.Y + half, half });
				square.Size = half;
			}
			corner = { square.X, square.Y };
			return true;
		}

	private:
		struct Square
		{
			uint32_t X = 0;
			uint32_t Y = 0;
			uint32_t Size = 0;
		};
		std::vector<Square> m_Free;
	};

}
