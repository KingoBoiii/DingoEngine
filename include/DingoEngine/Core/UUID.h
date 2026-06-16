#pragma once

#include <cstdint>
#include <functional>

namespace Dingo
{

	// A 64-bit randomly-generated unique identifier. Used by IDComponent to give
	// every entity a stable handle that survives across scenes, (de)serialization
	// and — eventually — network replication.
	class UUID
	{
	public:
		UUID();
		UUID(uint64_t uuid);
		UUID(const UUID&) = default;

		operator uint64_t() const { return m_UUID; }

	private:
		uint64_t m_UUID;
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
