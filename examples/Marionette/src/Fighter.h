#pragma once
#include "GameAssets.h"
#include "Moveset.h"

#include <DingoEngine.h>

namespace Dingo
{

	// The pack's characters face +Z, where the engine's own forward is -Z.
	class Fighter
	{
	public:
		Fighter(Scene& scene, const FighterDef& def, const GameAssets& assets, const glm::vec3& position, float yawDegrees);

		Entity GetEntity() const { return m_Entity; }
		Animator* GetAnimator() const;

		void ShowIdle(float time, bool freeze);

		// Rest-pose model-space height, before the fighter's scale.
		float GetModelHeight() const { return m_ModelHeight; }

	private:
		void SpawnWeapon(const char* path, const char* joint);

	private:
		Scene& m_Scene;
		const FighterDef& m_Def;
		const GameAssets& m_Assets;
		Entity m_Entity;
		float m_ModelHeight = 0.0f;
	};

}
