#include "ArenaDirector.h"
#include "ArenaWorld.h"
#include "CameraRig.h"
#include "Fighter.h"
#include "GameAssets.h"
#include "GameTuning.h"
#include "LaunchOptions.h"
#include "Moveset.h"

#include <algorithm>

namespace Dingo
{

	ArenaDirectorScript::ArenaDirectorScript(const GameAssets* assets)
		: m_Assets(assets)
	{}

	ArenaDirectorScript::~ArenaDirectorScript() = default;

	void ArenaDirectorScript::OnStart()
	{
		const LaunchOptions& options = GetLaunchOptions();
		Scene& scene = GetScene();

		m_World = std::make_unique<ArenaWorld>(scene);

		const std::span<const FighterDef> defs = GetFighterDefs();
		const float first = -0.5f * LINEUP_SPACING * static_cast<float>(defs.size() - 1);
		float tallest = 0.0f;
		for (size_t i = 0; i < defs.size(); ++i)
		{
			const glm::vec3 position(first + LINEUP_SPACING * static_cast<float>(i), 0.0f, 0.0f);
			auto fighter = std::make_unique<Fighter>(scene, defs[i], *m_Assets, position, 0.0f);
			fighter->ShowIdle(options.Freeze ? FREEZE_POSE_TIME : LINEUP_IDLE_STAGGER * static_cast<float>(i), options.Freeze);
			tallest = std::max(tallest, fighter->GetModelHeight() * defs[i].Scale);
			m_Fighters.push_back(std::move(fighter));
		}

		if (options.Overview)
		{
			m_Camera = std::make_unique<CameraRig>(scene, glm::vec3(0.0f), OVERVIEW_PITCH_DEG, m_World->GetRimPoints());
		}
		else
		{
			const float half = std::abs(first) + LINEUP_MARGIN;
			const float top = std::max(tallest, 0.5f * LINEUP_FIT_HEIGHT);
			std::vector<glm::vec3> fit;
			for (const float x : { -half, half })
				for (const float y : { 0.0f, top })
					fit.emplace_back(x, y, 0.0f);
			m_Camera = std::make_unique<CameraRig>(scene, glm::vec3(0.0f, LINEUP_LOOK_HEIGHT, 0.0f), LINEUP_PITCH_DEG, std::move(fit));
		}

		DE_INFO("Marionette: lineup of {} fighters{}{}", defs.size(), options.Freeze ? ", frozen" : "", options.Overview ? ", overview camera" : "");
	}

	void ArenaDirectorScript::OnUpdate(float)
	{
		if (Input::IsKeyPressed(Key::Escape) || Input::IsGamepadButtonPressed(GamepadButton::Start))
			RequestSceneTransition(SCENE_TITLE);
#ifdef DE_DEBUG
		else if (Input::IsKeyPressed(Key::End))
			RequestSceneTransition(SCENE_END);
#endif

		m_Camera->Update();
	}

	void ArenaDirectorScript::OnDestroy()
	{
		m_Camera.reset();
		m_Fighters.clear();
		m_World.reset();
	}

}
