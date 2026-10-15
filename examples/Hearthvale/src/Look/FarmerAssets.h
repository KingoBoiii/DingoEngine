#pragma once
#include <DingoEngine.h>

namespace Dingo
{

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

	private:
		Model* m_Model = nullptr;
		Material* m_Material = nullptr;
		const AnimationClip* m_Idle = nullptr;
		const AnimationClip* m_Walk = nullptr;
		float m_ModelHeight = 0.0f;
	};

}
