#pragma once
#include <memory>

namespace Dingo
{

	/// Ownership: Shared ownership; multiple shared_ptrs can point to the same object.
	/// Reference Counting : Internally, shared_ptr maintains a reference count(an integer counter).Each time a shared_ptr is copied, the count increases.When a shared_ptr is destroyed, the count decreases.When the count reaches zero(no owners left), the object is deleted.
	/// Copy : Can be freely copied.
	/// When to use: When multiple parts of your code need shared access to an object.
	template<typename T>
	using Ref = std::shared_ptr<T>;
	template<typename T, typename... Args>
	inline Ref<T> CreateRef(Args&&... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}

	/// Ownership: Sole ownership of a dynamically allocated object.
	/// Deletion: Deletes the object when the unique_ptr goes out of scope.
	/// Copy: Cannot be copied; can only be moved(ownership transfer via move semantics).
	/// When to use: When only one owner should manage the lifetime of the object.
	template<typename T>
	using Unique = std::unique_ptr<T>;
	template<typename T, typename... Args>
	inline Unique<T> CreateUnique(Args&&... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	/// Ownership: Does not own the object; it observes a shared_ptr but does not affect its reference count.
	/// Purpose: Prevents circular references(which can cause memory leaks with shared_ptr alone).
	/// When to use : When you need a non - owning reference to an object managed by shared_ptr, such as in observer patterns or to break cycles in graphs.
	template<typename T>
	using Weak = std::weak_ptr<T>;
	template<typename T>
	inline Weak<T> CreateWeak(const Ref<T>& ref)
	{
		return Weak<T>(ref);
	}

}
