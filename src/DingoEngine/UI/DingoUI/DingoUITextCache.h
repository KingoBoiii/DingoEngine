#pragma once
#include <glm/glm.hpp>

#include <bit>
#include <cstdint>
#include <functional>
#include <string_view>
#include <unordered_map>

namespace Dingo::Internal
{

	// Text sizes by (font atlas id, size, wrap width, text hash), in two generations: Advance()
	// drops what neither this frame nor the last one asked for. A hit allocates nothing.
	class UITextMetricsCache
	{
	public:
		template<typename ComputeFn>
		glm::vec2 Get(uint64_t fontId, float size, float maxWidth, std::string_view text, ComputeFn&& compute)
		{
			const Key key{ fontId, std::bit_cast<uint32_t>(size), std::bit_cast<uint32_t>(maxWidth), std::hash<std::string_view>{}(text) };
			if (const auto it = m_Current.find(key); it != m_Current.end())
			{
				++m_Hits;
				return it->second;
			}
			if (const auto it = m_Previous.find(key); it != m_Previous.end())
			{
				++m_Hits;
				return m_Current.emplace(key, it->second).first->second;
			}
			++m_Misses;
			return m_Current.emplace(key, compute()).first->second;
		}

		void Advance()
		{
			m_Previous.swap(m_Current);
			m_Current.clear();
		}

		size_t Size() const { return m_Current.size() + m_Previous.size(); }
		uint64_t Hits() const { return m_Hits; }
		uint64_t Misses() const { return m_Misses; }

	private:
		struct Key
		{
			uint64_t FontId;
			uint32_t SizeBits;
			uint32_t WidthBits;
			size_t TextHash;

			bool operator==(const Key&) const = default;
		};

		struct KeyHash
		{
			size_t operator()(const Key& key) const
			{
				uint64_t h = key.TextHash ^ (key.FontId * 0x9E3779B97F4A7C15ull);
				h = (h ^ ((static_cast<uint64_t>(key.SizeBits) << 32) | key.WidthBits)) * 0xBF58476D1CE4E5B9ull;
				return static_cast<size_t>(h ^ (h >> 31));
			}
		};

		using Map = std::unordered_map<Key, glm::vec2, KeyHash>;

		Map m_Current;
		Map m_Previous;
		uint64_t m_Hits = 0;
		uint64_t m_Misses = 0;
	};

}
