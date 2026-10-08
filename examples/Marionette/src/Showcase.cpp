#include "Showcase.h"
#include "ArenaVfx.h"
#include "ArenaWorld.h"
#include "CameraRig.h"
#include "Fighter.h"
#include "GameAssets.h"
#include "GameTuning.h"
#include "LaunchOptions.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <utility>

namespace Dingo
{

	Showcase::Showcase(Scene& scene, const GameAssets& assets, const ShowcaseParams& params)
		: m_Assets(assets), m_EventGeneration(assets.GetEventGeneration())
	{
		m_Audio = std::make_unique<GameAudio>(assets.GetSounds());
		if (!GetLaunchOptions().NoParticles)
			m_Vfx = std::make_unique<ArenaVfx>(scene, false);
		m_World = std::make_unique<ArenaWorld>(scene, assets, m_Audio.get(), m_Vfx.get());

		if (params.Kind == ShowcaseKind::Victory)
			BuildVictory(scene, assets, params);
		else
			BuildRow(scene, assets, params);
	}

	Showcase::~Showcase() = default;

	void Showcase::Update(float deltaTime)
	{
		m_Time += deltaTime;
		if (m_EventGeneration != m_Assets.GetEventGeneration())
		{
			m_EventGeneration = m_Assets.GetEventGeneration();
			for (const std::unique_ptr<Fighter>& fighter : m_Fighters)
				fighter->OnEventsChanged();
		}
		if (m_Camera)
			m_Camera->Update();
	}

	void Showcase::BuildRow(Scene& scene, const GameAssets& assets, const ShowcaseParams& params)
	{
		const FighterContext context{ scene, assets, *m_Audio, m_Time, false, false, params.Debug };
		const std::span<const FighterDef> defs = GetFighterDefs();
		const float first = -0.5f * LINEUP_SPACING * static_cast<float>(defs.size() - 1);
		float tallest = 0.0f;
		for (size_t i = 0; i < defs.size(); ++i)
		{
			FighterSpawn spawn;
			spawn.Position = glm::vec3(first + LINEUP_SPACING * static_cast<float>(i), 0.0f, 0.0f);
			spawn.Controlled = false;
			auto fighter = std::make_unique<Fighter>(context, defs[i], spawn);
			fighter->ShowIdle(params.Freeze ? FREEZE_POSE_TIME : LINEUP_IDLE_STAGGER * static_cast<float>(i), params.Freeze);
			tallest = std::max(tallest, fighter->GetModelHeight() * defs[i].Scale);
			m_Fighters.push_back(std::move(fighter));
		}

		if (params.Overview)
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
			const float center = params.Kind == ShowcaseKind::Title ? TITLE_LINEUP_CENTER_NDC : 0.0f;
			m_Camera = std::make_unique<CameraRig>(scene, glm::vec3(0.0f, LINEUP_LOOK_HEIGHT, 0.0f), LINEUP_PITCH_DEG, std::move(fit), center);
		}

		if (params.Kind == ShowcaseKind::Lineup)
			DE_INFO("Marionette: lineup of {} fighters{}{}", defs.size(), params.Freeze ? ", frozen" : "", params.Overview ? ", overview camera" : "");
	}

	void Showcase::BuildVictory(Scene& scene, const GameAssets& assets, const ShowcaseParams& params)
	{
		const FighterContext context{ scene, assets, *m_Audio, m_Time, false, false, params.Debug };
		const FighterDef& def = GetPlayerDef();
		auto fighter = std::make_unique<Fighter>(context, def, FighterSpawn{ glm::vec3(0.0f), 0.0f, false });

		const AnimationClip* taunt = params.Taunt ? assets.GetClip(Clips::TAUNT) : nullptr;
		if (taunt)
			fighter->ShowClip(taunt, 0.0f, false);
		else
			fighter->ShowIdle(0.0f, false);

		const float top = std::max(VICTORY_FIT_HEIGHT, fighter->GetModelHeight() * def.Scale + ARENA_CAMERA_HEAD_MARGIN);
		std::vector<glm::vec3> fit;
		for (const float x : { -VICTORY_FIT_HALF_WIDTH, VICTORY_FIT_HALF_WIDTH })
			for (const float y : { -VICTORY_FIT_FLOOR, top })
				fit.emplace_back(x, y, 0.0f);
		m_Camera = std::make_unique<CameraRig>(scene, glm::vec3(0.0f, VICTORY_LOOK_HEIGHT, 0.0f), VICTORY_PITCH_DEG, std::move(fit), VICTORY_CENTER_NDC);
		m_Fighters.push_back(std::move(fighter));
	}

}
