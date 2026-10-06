#pragma once

#include <cstdint>
#include <functional>

namespace Dingo
{

	// A 64-bit unique identifier. Used by IDComponent to give every entity a stable
	// handle that survives across scenes, (de)serialization and — eventually — network
	// replication, and by the AssetManager as AssetHandle.
	//
	// A default-constructed UUID is the null id 0, so an uninitialised member or a
	// resize()d vector slot reads as "no id" rather than as a random, valid-looking one.
	// Fresh ids come from Generate(), which never returns 0.
	class UUID
	{
	public:
		constexpr UUID() = default;
		constexpr UUID(uint64_t uuid) : m_UUID(uuid) {}
		constexpr UUID(const UUID&) = default;
		constexpr UUID& operator=(const UUID&) = default;

		static UUID Generate();

		constexpr operator uint64_t() const { return m_UUID; }

	private:
		uint64_t m_UUID = 0;
	};

}

namespace std
{

	template<>
	struct hash<Dingo::UUID>
	{
		std::size_t operator()(const Dingo::UUID& uuid) const noexcept
		{
			return static_cast<std::size_t>(static_cast<uint64_t>(uuid));
		}
	};

}
