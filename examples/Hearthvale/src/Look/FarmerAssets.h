#pragma once
#include <DingoEngine.h>

#include <string>
#include <string_view>
#include <vector>

namespace Dingo
{

	// The model, texture and two clip libraries the farmer loads, relative to the asset root.
	std::vector<const char*> FarmerAssetPaths();

	// A KayKit character as the villagers use it: the model, its texture's lit material and its height.
	struct CharacterAsset
	{
		std::string Path;
		Model* Loaded = nullptr;
		Material* Lit = nullptr;
		float Height = 0.0f;
	};

	// The KayKit farmer: the character model, its texture and lit material, and the idle and walk clips retargeted
	// from two clips-only libraries. GPU side: never constructed headless.
	class FarmerAssets
	{
	public:
		FarmerAssets();
		~FarmerAssets();

		FarmerAssets(const FarmerAssets&) = delete;
		FarmerAssets& operator=(const FarmerAssets&) = delete;

		// True when the model has a skeleton and both clips resolved.
		bool IsUsable() const { return m_Model && m_Model->GetSkeleton() && m_Idle && m_Walk; }

		Model* GetModel() const { return m_Model; }
		Material* GetMaterial() const { return m_Material; }
		const AnimationClip* GetIdle() const { return m_Idle; }
		const AnimationClip* GetWalk() const { return m_Walk; }
		float GetModelHeight() const { return m_ModelHeight; }

		// The farmer's model and the villagers' other character, by model path; null when it is neither.
		const CharacterAsset* FindCharacter(std::string_view modelPath) const;

	private:
		Model* m_Model = nullptr;
		Material* m_Material = nullptr;
		const AnimationClip* m_Idle = nullptr;
		const AnimationClip* m_Walk = nullptr;
		float m_ModelHeight = 0.0f;
		std::vector<CharacterAsset> m_Characters;
	};

}
