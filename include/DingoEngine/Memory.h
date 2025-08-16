#pragma once
#include <memory>

namespace Dingo
{

	template<typename T>
	using Ref = std::shared_ptr<T>;
	template<typename T, typename... Args>
	inline Ref<T> CreateRef(Args&&... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}

	template<typename T>
	using Unique = std::unique_ptr<T>;
	template<typename T, typename... Args>
	inline Unique<T> CreateUnique(Args&&... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

}
