#include "HearthvaleLayer.h"
#include "Checks/Checks.h"
#include "GameTuning.h"
#include "LaunchOptions.h"
#include "Life/PlayerInput.h"
#include "Look/ArtRegistry.h"
#include "World/ValleyLayout.h"

namespace Dingo
{

	HearthvaleLayer::HearthvaleLayer()
		: Layer("Hearthvale")
	{}

	HearthvaleLayer::~HearthvaleLayer() = default;

	void HearthvaleLayer::OnAttach()
	{
		const LaunchOptions& options = GetLaunchOptions();
		m_Director = std::make_unique<ValleyDirector>(MakeDirectorConfig(options));
		m_Director->OnDayRoll(DayRollStep::Crops, [this](ValleyDirector&, DayRollReason)
		{
			if (m_World)
				m_World->OnDayRoll();
		});
		if (!Application::Get().IsHeadless())
		{
			Renderer3D& renderer = Application::Get().GetRenderer3D();
			renderer.SetCullingEnabled(!options.NoCull);
			renderer.SetInstancingEnabled(!options.NoInstancing);
		}
		m_ValleyScene = m_SceneManager.CreateScene(SCENE_VALLEY);
		RegisterStages();
		BuildValleyScene();
		m_SceneManager.SetActiveScene(SCENE_VALLEY);
	}

	void HearthvaleLayer::OnDetach()
	{
		m_StaticChecks.reset();
		ReleaseScenePieces();
		m_World.reset();
		if (Scene* active = m_SceneManager.GetActiveScene())
			active->OnStop();
		m_SceneManager.Clear();
		m_FarmerAssets.reset();
		m_ValleyScene = nullptr;
		m_Director.reset();
	}

	void HearthvaleLayer::ReleaseScenePieces()
	{
		m_Hud.reset();
		m_Camera.reset();
		m_Player.reset();
		m_Sky.reset();
	}

	void HearthvaleLayer::RegisterStages()
	{
		m_Director->OnStage(SimStage::PlayerIntent, [this](ValleyDirector& director, const StepInfo& step)
		{
			if (m_Player)
				m_Player->Step(m_Intent, director.GetGrid(), m_Camera ? m_Camera->GetYaw() : 0.0f, step.Dt);
		});
		m_Director->OnStage(SimStage::Sky, [this](ValleyDirector& director, const StepInfo& step)
		{
			if (m_Sky)
				m_Sky->Update(step.Dt, director.GetCalendar().GetHours(), director.GetCalendar().GetWeather());
		});
		m_Director->OnStage(SimStage::Camera, [this](ValleyDirector&, const StepInfo& step)
		{
			if (m_Camera && m_Player)
				m_Camera->Update(step.Dt, m_Player->GetFeet(), m_Intent.CameraSteps, m_Intent.CameraZoomAxis, m_Intent.CameraZoomWheel);
		});
		m_Director->OnStage(SimStage::Ui, [this](ValleyDirector&, const StepInfo&)
		{
			m_Intent.ClearPulses();
		});
	}

	void HearthvaleLayer::BuildValleyScene()
	{
		ReleaseScenePieces();
		m_World.reset();
		m_ValleyScene->Clear();
		if (Application::Get().IsHeadless())
			return;

		const LaunchOptions& options = GetLaunchOptions();
		const Calendar& calendar = m_Director->GetCalendar();
		m_Sky = std::make_unique<Sky>(*m_ValleyScene, !options.NoShadows, !options.NoFog);
		m_Sky->Update(0.0f, calendar.GetHours(), calendar.GetWeather());

		ValleyWorldOptions worldOptions;
		worldOptions.Static = !options.NoStatic;
		m_World = std::make_unique<ValleyWorld>(*m_ValleyScene, m_Director->GetGrid(), worldOptions);
		m_World->Sync();

		Player::SpawnFloor(*m_ValleyScene, m_Director->GetGrid().GetWidth(), m_Director->GetGrid().GetHeight());

		if (options.FarmerModel)
		{
			m_FarmerAssets = std::make_unique<FarmerAssets>();
			if (!m_FarmerAssets->IsUsable())
			{
				DE_WARN("Hearthvale: the farmer model did not load; using the placeholder body");
			}
		}

		const LandmarkSpec& home = GetLandmarkSpec(Landmark::FarmHouse);
		const glm::vec3 feet((static_cast<float>(home.DoorX) + 0.5f) * TILE_SIZE, GROUND_TOP + PLAYER_SPAWN_LIFT,
			(static_cast<float>(home.DoorY) + 1.5f) * TILE_SIZE);
		m_Player = std::make_unique<Player>(*m_ValleyScene, feet, m_FarmerAssets.get());
		m_Camera = std::make_unique<CameraRig>(*m_ValleyScene, feet);
		m_Hud = std::make_unique<ClockHud>();
	}

	void HearthvaleLayer::RunChecks()
	{
		if (!m_SimChecksDone)
		{
			m_SimChecksDone = true;
			RunSimChecks(m_Report);
			if (!Application::Get().IsHeadless() && m_World)
				m_StaticChecks = std::make_unique<StaticCheckRun>();
		}
		if (m_StaticChecks && !m_StaticChecks->Tick(m_Report, *m_Director, *m_World, Application::Get().GetRenderer3D().GetStatistics().StaticCellsBaked))
			return;

		m_StaticChecks.reset();
		m_ChecksDone = true;
		DE_INFO("[Checks] {} passed, {} failed", m_Report.GetPassed(), m_Report.GetFailed());
		if (Application::Get().IsHeadless())
			Application::Get().Close();
	}

	void HearthvaleLayer::OnUpdate(float deltaTime)
	{
		const LaunchOptions& options = GetLaunchOptions();

		if (options.Check && !m_ChecksDone)
		{
			RunChecks();
			if (m_ChecksDone && Application::Get().IsHeadless())
				return;
		}

		if (m_Player && !options.Autoplay)
			PollPlayerIntent(m_Intent);

		const float step = options.FixedDt > 0.0f ? options.FixedDt : deltaTime;
		const int steps = options.FixedDt > 0.0f ? options.StepsPerFrame : 1;
		for (int i = 0; i < steps; ++i)
		{
			m_Director->Advance(step, SIM_MAX_STEPS_REAL_TIME);
			m_SceneManager.OnUpdate(step);
		}
		if (m_World)
			m_World->Sync();
		if (m_Hud)
			m_Hud->Draw(m_Director->GetCalendar());
		m_SceneManager.OnRender();
	}

}
