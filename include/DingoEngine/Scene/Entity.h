#pragma once

#include "DingoEngine/Core/UUID.h"
#include "DingoEngine/Scene/Components.h"

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

namespace Dingo
{

	class Scene;
	class ScriptableEntity;

	// A lightweight, copyable handle to an entity inside a Scene. The underlying
	// ECS (EnTT) is fully hidden: the handle is an opaque integer and all access
	// is routed through the engine. Client code never sees or links EnTT.
	class Entity
	{
	public:
		Entity() = default;

		// --- Components (built-in component types only) ---

		template<typename T>
		T& AddComponent(const T& component = T{});

		template<typename T>
		T& GetComponent();

		template<typename T>
		bool HasComponent() const;

		template<typename T>
		void RemoveComponent();

		// --- Behaviours ---

		// Attach a script (must derive from ScriptableEntity). The scene owns the
		// instance and drives its OnCreate/OnUpdate/OnDestroy.
		template<typename T, typename... Args>
		T& AddScript(Args&&... args)
		{
			static_assert(std::is_base_of_v<ScriptableEntity, T>, "T must derive from ScriptableEntity");
			T* instance = new T(std::forward<Args>(args)...);
			AttachScript(static_cast<ScriptableEntity*>(instance));
			return *instance;
		}

		template<typename T>
		T* GetScript()
		{
			return dynamic_cast<T*>(GetScriptInstance());
		}

		template<typename T>
		bool HasScript()
		{
			return GetScript<T>() != nullptr;
		}

		// --- Identity / lifetime ---

		UUID GetUUID() const;
		const std::string& GetName() const;
		Scene& GetScene() const;

		bool IsValid() const;
		void Destroy();

		explicit operator bool() const { return IsValid(); }
		bool operator==(const Entity& other) const { return m_Handle == other.m_Handle && m_Scene == other.m_Scene; }
		bool operator!=(const Entity& other) const { return !(*this == other); }

	private:
		Entity(std::uint32_t handle, Scene* scene) : m_Handle(handle), m_Scene(scene) {}

		void AttachScript(ScriptableEntity* instance);
		ScriptableEntity* GetScriptInstance();

	private:
		static constexpr std::uint32_t k_InvalidHandle = 0xFFFFFFFFu;

		std::uint32_t m_Handle = k_InvalidHandle;
		Scene* m_Scene = nullptr;

		friend class Scene;
	};

}
