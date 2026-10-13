#pragma once

#include <cstdint>
#include <functional>

namespace Dingo
{

	/// A 64-bit unique identifier. Used by IDComponent to give every entity a stable
	/// handle that survives across scenes, (de)serialization and - eventually - network
	/// replication, and by the AssetManager as AssetHandle.
	///
	/// A default-constructed UUID is the null id 0, so an uninitialised member or a
	/// resize()d vector slot reads as "no id" rather than as a random, valid-looking one.
	/// Fresh ids come from Generate(), which never returns 0.
	class UUID
	{
	public:
		/// The null id, 0.
		constexpr UUID() = default;
		/// Wraps a raw value, e.g. one read back from a save; 0 is the null id.
		explicit constexpr UUID(uint64_t uuid) : m_UUID(uuid) {}
		/// Copies the id.
		constexpr UUID(const UUID&) = default;
		/// Copies the id.
		constexpr UUID& operator=(const UUID&) = default;

		/// A new random non-zero id. Draws from one shared generator without locking, so call it
		/// from one thread at a time.
		static UUID Generate();

		/// The raw 64-bit value; 0 for the null id, so a UUID also works as a "has an id" test.
		constexpr operator uint64_t() const { return m_UUID; }

	private:
		uint64_t m_UUID = 0;
	};

}

namespace std
{

	/// Hashes the 64-bit value, so a UUID can key a std::unordered_map or std::unordered_set.
	template<>
	struct hash<Dingo::UUID>
	{
		/// The id's value as a hash.
		std::size_t operator()(const Dingo::UUID& uuid) const noexcept
		{
			return static_cast<std::size_t>(static_cast<uint64_t>(uuid));
		}
	};

}
