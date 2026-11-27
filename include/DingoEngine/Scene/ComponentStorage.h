#pragma once

#include "DingoEngine/Scene/Components.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

namespace Dingo
{

	/// Settings for a game component type T (any type Entity::AddComponent takes besides the
	/// built-in ones). Specialise it for a type whose copy constructor exists but can't compile, such
	/// as a struct holding a std::vector<std::unique_ptr<...>>.
	template<typename T>
	struct ComponentTraits
	{
		/// True when Scene::DuplicateEntity copies a T to the clone. Default: T is copy-constructible.
		static constexpr bool Copyable = std::is_copy_constructible_v<T>;
	};

	namespace Internal
	{

		template<typename... Ts>
		struct TypeList
		{
			static constexpr std::size_t Size = sizeof...(Ts);
		};

		template<typename T, typename List>
		struct TypeListContains;

		template<typename T, typename... Ts>
		struct TypeListContains<T, TypeList<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

		// Entity::AddComponent and friends send these to the EnTT registry and every other type to a
		// CustomComponentPool. A new built-in component goes here as well as in DE_INSTANTIATE_COMPONENT
		// (Entity.cpp), whose static_assert checks the two lists agree.
		using BuiltinComponents = TypeList<
			IDComponent, TagComponent, TransformComponent, SpriteRendererComponent, CircleRendererComponent,
			TextComponent, CameraComponent, PostProcessComponent, RigidBody2DComponent, BoxCollider2DComponent,
			CircleCollider2DComponent, Transform3DComponent, MeshRendererComponent, SkinnedMeshRendererComponent,
			AnimatorComponent, ParticleEmitterComponent, ParticleEventComponent, RigidBody3DComponent,
			Constraint3DComponent, RagdollComponent, BoxCollider3DComponent, SphereCollider3DComponent,
			CapsuleCollider3DComponent, MeshCollider3DComponent, CharacterController3DComponent,
			DirectionalLightComponent, AmbientLightComponent, PointLightComponent, SpotLightComponent,
			FogComponent, SkyboxComponent, FogVolumeComponent, AudioSourceComponent, AudioListenerComponent>;

		template<typename T>
		inline constexpr bool IsBuiltinComponent = TypeListContains<std::remove_cv_t<T>, BuiltinComponents>::value;

		std::uint32_t NextCustomComponentIndex();
		// Logs `message` as an error and asserts in Debug.
		void ReportCustomComponentError(const char* message);
		// Logs `message` as an error only.
		void LogCustomComponentError(const char* message);

		template<typename T>
		std::uint32_t CustomComponentIndex()
		{
			static const std::uint32_t s_Index = NextCustomComponentIndex();
			return s_Index;
		}

		// The low bits of an entity handle are its slot, the rest its version (EnTT's default
		// layout, checked in Entity.cpp). A slot is reused by a later entity with a new version.
		inline constexpr std::uint32_t k_EntitySlotMask = 0xFFFFFu;
		inline constexpr std::uint32_t k_NoIndex = 0xFFFFFFFFu;
		// Marks a dense entry removed while the pool is locked; no live handle has this value.
		inline constexpr std::uint32_t k_RemovedHandle = 0xFFFFFFFFu;

		// A sparse set: m_Sparse maps a slot to a dense index, the dense arrays hold the handles and
		// the components in iteration order. While an Each holds the pool locked, removals only mark
		// their entry (nothing moves, so the loop's indices stay good) and land when it unlocks;
		// additions append, which PagedStorage does without moving the others.
		class CustomComponentPoolBase
		{
		public:
			virtual ~CustomComponentPoolBase() = default;

			bool Contains(std::uint32_t handle) const
			{
				if (handle == k_RemovedHandle)
					return false;
				const std::uint32_t slot = handle & k_EntitySlotMask;
				return slot < m_Sparse.size() && m_Sparse[slot] != k_NoIndex && m_Handles[m_Sparse[slot]] == handle;
			}

			std::size_t DenseSize() const { return m_Handles.size(); }
			std::size_t Count() const { return m_Handles.size() - m_RemovedCount; }
			std::uint32_t HandleAt(std::size_t index) const { return m_Handles[index]; }
			std::uint32_t IndexOf(std::uint32_t handle) const { return m_Sparse[handle & k_EntitySlotMask]; }
			bool IsLocked() const { return m_Locks > 0; }

			void Remove(std::uint32_t handle)
			{
				if (!Contains(handle))
					return;
				const std::uint32_t index = IndexOf(handle);
				m_Sparse[handle & k_EntitySlotMask] = k_NoIndex;
				if (m_Locks > 0)
				{
					m_Handles[index] = k_RemovedHandle;
					++m_RemovedCount;
					return;
				}
				EraseAt(index);
			}

			void Clear()
			{
				std::fill(m_Sparse.begin(), m_Sparse.end(), k_NoIndex);
				if (m_Locks > 0)
				{
					std::fill(m_Handles.begin(), m_Handles.end(), k_RemovedHandle);
					m_RemovedCount = m_Handles.size();
					return;
				}
				m_Handles.clear();
				m_RemovedCount = 0;
				ClearComponents();
			}

			void Lock() { ++m_Locks; }

			void Unlock()
			{
				if (--m_Locks > 0 || m_RemovedCount == 0)
					return;
				// Highest first: each erase swaps in the last entry, which is then never a marked one.
				for (std::size_t index = m_Handles.size(); index-- > 0;)
				{
					if (m_Handles[index] == k_RemovedHandle)
						EraseAt(static_cast<std::uint32_t>(index));
				}
				m_RemovedCount = 0;
			}

			virtual void Copy(std::uint32_t source, std::uint32_t destination) = 0;

		protected:
			std::uint32_t Insert(std::uint32_t handle)
			{
				const std::uint32_t slot = handle & k_EntitySlotMask;
				if (slot >= m_Sparse.size())
					m_Sparse.resize(static_cast<std::size_t>(slot) + 1, k_NoIndex);
				const std::uint32_t index = static_cast<std::uint32_t>(m_Handles.size());
				m_Sparse[slot] = index;
				m_Handles.push_back(handle);
				return index;
			}

			void EraseAt(std::uint32_t index)
			{
				const std::uint32_t last = static_cast<std::uint32_t>(m_Handles.size() - 1);
				if (index != last)
				{
					MoveComponent(last, index);
					m_Handles[index] = m_Handles[last];
					if (m_Handles[index] != k_RemovedHandle)
						m_Sparse[m_Handles[index] & k_EntitySlotMask] = index;
				}
				m_Handles.pop_back();
				PopComponent();
			}

			virtual void MoveComponent(std::uint32_t from, std::uint32_t to) = 0;
			virtual void PopComponent() = 0;
			virtual void ClearComponents() = 0;

			std::vector<std::uint32_t> m_Sparse;
			std::vector<std::uint32_t> m_Handles;
			std::size_t m_RemovedCount = 0;
			std::uint32_t m_Locks = 0;
		};

		// Fixed-size pages, so an append never moves a component (a reference survives it) and
		// storage grows by one allocation per page, not per component.
		template<typename T>
		class PagedStorage
		{
		public:
			PagedStorage() = default;
			PagedStorage(const PagedStorage&) = delete;
			PagedStorage& operator=(const PagedStorage&) = delete;
			~PagedStorage() { Clear(); }

			std::size_t Size() const { return m_Size; }

			T& operator[](std::size_t index)
			{
				return *std::launder(reinterpret_cast<T*>(m_Pages[index / k_PageSize]->Bytes) + index % k_PageSize);
			}

			template<typename... Args>
			T& EmplaceBack(Args&&... args)
			{
				if (m_Size == m_Pages.size() * k_PageSize)
					m_Pages.push_back(std::make_unique<Page>());
				T* slot = std::launder(reinterpret_cast<T*>(m_Pages[m_Size / k_PageSize]->Bytes) + m_Size % k_PageSize);
				::new (static_cast<void*>(slot)) T(std::forward<Args>(args)...);
				++m_Size;
				return *slot;
			}

			void PopBack()
			{
				std::destroy_at(&(*this)[m_Size - 1]);
				--m_Size;
			}

			void Clear()
			{
				while (m_Size > 0)
					PopBack();
			}

		private:
			static constexpr std::size_t k_PageSize = sizeof(T) >= 1024 ? 16 : 256;
			struct alignas(T) Page { std::byte Bytes[sizeof(T) * k_PageSize]; };

			std::vector<std::unique_ptr<Page>> m_Pages;
			std::size_t m_Size = 0;
		};

		template<typename T>
		class CustomComponentPool final : public CustomComponentPoolBase
		{
			static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>, "A custom component must be move-constructible and move-assignable");

		public:
			template<typename... Args>
			T& Emplace(std::uint32_t handle, Args&&... args)
			{
				if (Contains(handle))
				{
					T& existing = m_Components[IndexOf(handle)];
					existing = T(std::forward<Args>(args)...);
					return existing;
				}
				T& stored = m_Components.EmplaceBack(std::forward<Args>(args)...);
				Insert(handle);
				return stored;
			}

			T& Get(std::uint32_t handle) { return m_Components[IndexOf(handle)]; }

			void Copy(std::uint32_t source, std::uint32_t destination) override
			{
				if constexpr (ComponentTraits<T>::Copyable)
				{
					if (Contains(source))
					{
						T copy = m_Components[IndexOf(source)];
						Emplace(destination, std::move(copy));
					}
				}
			}

			template<typename Compare>
			void Sort(Compare compare)
			{
				std::vector<std::uint32_t> order(m_Handles.size());
				std::iota(order.begin(), order.end(), 0u);
				// Stable, so equal keys keep their order and the result is the same on every standard library.
				std::stable_sort(order.begin(), order.end(), [&](std::uint32_t a, std::uint32_t b)
				{
					return compare(std::as_const(m_Components[a]), std::as_const(m_Components[b]));
				});

				std::vector<T> components;
				std::vector<std::uint32_t> handles;
				components.reserve(order.size());
				handles.reserve(order.size());
				for (std::uint32_t from : order)
				{
					components.push_back(std::move(m_Components[from]));
					handles.push_back(m_Handles[from]);
				}
				for (std::uint32_t index = 0; index < components.size(); ++index)
					m_Components[index] = std::move(components[index]);
				m_Handles = std::move(handles);
				for (std::uint32_t index = 0; index < m_Handles.size(); ++index)
					m_Sparse[m_Handles[index] & k_EntitySlotMask] = index;
			}

		protected:
			void MoveComponent(std::uint32_t from, std::uint32_t to) override { m_Components[to] = std::move(m_Components[from]); }
			void PopComponent() override { m_Components.PopBack(); }
			void ClearComponents() override { m_Components.Clear(); }

		private:
			PagedStorage<T> m_Components;
		};

		// What an invalid AddComponent or GetComponent returns after its error, so nothing is stored for
		// an entity that doesn't exist. Main thread only, as the scene is.
		template<typename T, typename... Args>
		T& ScratchComponent(Args&&... args)
		{
			static std::optional<T> s_Scratch;
			s_Scratch.emplace(std::forward<Args>(args)...);
			return *s_Scratch;
		}

	}

}
