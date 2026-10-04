#include "Fighter.h"

#include <algorithm>
#include <format>
#include <string>

namespace Dingo
{

	Fighter::Fighter(Scene& scene, const FighterDef& def, const GameAssets& assets, const glm::vec3& position, float yawDegrees)
		: m_Scene(scene), m_Def(def), m_Assets(assets)
	{
		m_Entity = scene.CreateEntity(def.Name);

		Model* model = assets.GetCharacter(def);
		if (!model || !model->GetSkeleton())
		{
			DE_ERROR("Marionette: {} has no skinned model; it will not be drawn", def.Name);
			return;
		}

		auto& transform = m_Entity.AddComponent<Transform3DComponent>();
		transform.Position = position;
		transform.Rotation = glm::angleAxis(glm::radians(yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
		transform.Scale = glm::vec3(def.Scale);

		m_Entity.AddComponent<SkinnedMeshRendererComponent>(SkinnedMeshRendererComponent(model)).Material = assets.GetMaterial(def);

		AnimatorComponent animator;
		animator.PlayOnStart = false;
		animator.Speed = def.Pace;
		m_Entity.AddComponent<AnimatorComponent>(animator);

		float top = 0.0f;
		for (const SubMesh& submesh : model->GetSubMeshes())
		{
			if (!submesh.MeshData)
				continue;
			for (const MeshVertex& vertex : submesh.MeshData->GetVertices())
				top = std::max(top, vertex.Position.y);
		}
		m_ModelHeight = top;

		if (def.RightWeapon)
			SpawnWeapon(def.RightWeapon, Joints::HAND_RIGHT);
		if (def.LeftWeapon)
			SpawnWeapon(def.LeftWeapon, Joints::HAND_LEFT);
	}

	Animator* Fighter::GetAnimator() const
	{
		return m_Scene.GetAnimator(m_Entity);
	}

	void Fighter::SpawnWeapon(const char* path, const char* joint)
	{
		const Model* weapon = m_Assets.GetModel(path);
		if (!weapon)
		{
			DE_ERROR("Marionette: {} cannot carry '{}', it did not load", m_Def.Name, path);
			return;
		}

		Material* material = m_Assets.GetMaterial(m_Def);
		const std::string name = std::format("{} {}", m_Def.Name, joint);
		size_t index = 0;
		for (const SubMesh& submesh : weapon->GetSubMeshes())
		{
			if (!submesh.MeshData)
				continue;

			Entity part = m_Scene.CreateEntity(index == 0 ? name : std::format("{} {}", name, index));
			++index;
			part.AddComponent<Transform3DComponent>();
			part.AddComponent<MeshRendererComponent>(MeshRendererComponent(submesh.MeshData)).Material = material;
			part.SetParent(m_Entity, joint, false);
		}
	}

	void Fighter::ShowIdle(float time, bool freeze)
	{
		Animator* animator = GetAnimator();
		const AnimationClip* idle = m_Assets.GetClip(Clips::IDLE);
		if (!animator || !idle)
			return;

		animator->Play(idle);
		animator->SetTime(time);
		animator->Evaluate();
		m_Entity.GetComponent<AnimatorComponent>().Enabled = !freeze;
	}

}
