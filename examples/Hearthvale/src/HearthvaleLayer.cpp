#include "HearthvaleLayer.h"
#include "Checks/Checks.h"
#include "Checks/LifeWorldChecks.h"
#include "GameTuning.h"
#include "LaunchOptions.h"
#include "Life/PlayerInput.h"
#include "Life/PlayerMath.h"
#include "Look/ArtRegistry.h"
#include "Look/NightCurve.h"
#include "Placing/Interactions.h"
#include "Placing/PlaceInteract.h"
#include "World/ValleyLayout.h"

#include <algorithm>
#include <cmath>

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
		m_Farm = std::make_unique<FarmSim>(*m_Director, m_State.Pack);
		m_Economy = std::make_unique<EconomySim>(*m_Director, m_State);
		m_Villagers = std::make_unique<VillagerSim>(*m_Director);
		m_AnimalSim = std::make_unique<AnimalSim>(*m_Director, m_State.Animals);
		m_Dialogue = std::make_unique<VillagerDialogue>(*m_Director, m_State);
		m_Sleep = std::make_unique<SleepFlow>(*m_Director);
		m_Build = std::make_unique<BuildController>(m_Director->GetGrid(), m_State.Pack, [this](const TileRect& area)
		{
			if (m_World)
				m_World->MarkArea(area.X, area.Y, area.W, area.H);
			if (m_Villagers)
				m_Villagers->InvalidatePlans();
		});
		m_Build->SetCalendar(&m_Director->GetCalendar());
		m_Build->SetSleepHandler([this] { m_Sleep->RequestSleep(); });
		m_Build->SetFeedbackHandler([this](ActionFeedback feedback, int x, int y) { OnActionFeedback(feedback, x, y); });
		m_State.Pack.GiveStarterLoadout();
		m_Saves = std::make_unique<GameSaves>(*m_Director, m_State, SaveStore(SaveSlots::GetDirectory()));
		if (options.HasSeed)
			m_Saves->SetNewGameSeed(options.Seed);
		m_Saves->SetOnGameReplaced([this] { OnGameReplaced(); });
		if (!options.Load.empty())
			LoadAtLaunch(options.Load);
		m_Screens = std::make_unique<ScreenHost>(*m_Director, m_State.Pack, *m_Build);
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
		SetUpMenus();
	}

	void HearthvaleLayer::LoadAtLaunch(const std::string& slotText)
	{
		const int slot = GameSaves::ParseSlot(slotText);
		std::string message;
		if (slot < 0)
			DE_WARN("Hearthvale: --load={} names no slot (1 to {} or slot1 to slot{}); starting a new game", slotText, SlotProvider::SLOT_COUNT, SlotProvider::SLOT_COUNT);
		else if (!m_Saves->Load(slot, message))
			DE_WARN("Hearthvale: --load: {}; starting a new game", message);
		else
		{
			DE_INFO("Hearthvale: {}", message);
			m_LoadedAtLaunch = true;
		}
	}

	void HearthvaleLayer::OnGameReplaced()
	{
		if (m_Build)
			m_Build->SetMode(BuildMode::Tools);
		if (m_Villagers)
			m_Villagers->InvalidatePlans();
		if (m_AnimalSim)
			m_AnimalSim->Reset(m_Director->GetSeed());
		if (m_World)
			m_World->MarkAll();
		m_LightsSeenCells = ~0ull;
		if (m_Player)
		{
			const LandmarkSpec& home = GetLandmarkSpec(Landmark::FarmHouse);
			m_Player->Teleport(glm::vec3((static_cast<float>(home.DoorX) + 0.5f) * TILE_SIZE, GROUND_TOP + PLAYER_SPAWN_LIFT,
				(static_cast<float>(home.DoorY) + 1.5f) * TILE_SIZE));
		}
		m_LastCoins = m_State.Pack.GetCoins();
		m_LastSlot = m_State.Pack.GetSelectedSlot();
	}

	void HearthvaleLayer::SetUpMenus()
	{
		const LaunchOptions& options = GetLaunchOptions();
		const bool interactive = !IsScripted(options);
		Application& app = Application::Get();

		if (!app.IsHeadless())
		{
			AudioEngine& audio = app.GetAudioEngine();
			for (const char* bus : { k_BusMusic, k_BusAmbience, k_BusSfx, k_BusUi })
			{
				if (audio.FindBus(bus) == k_InvalidBus)
					(void)audio.CreateBus(bus);
			}
		}

		const Settings defaults = OptionsToSettings(OptionsState{}, Settings{});
		m_SettingsBase = interactive ? Settings::Load() : defaults;

		MenuServices services;
		services.State = &m_State;
		services.Slots = m_Saves.get();
		services.OnQuit = [] { Application::Get().Close(); };
		services.OnClick = [this]
		{
			if (m_Audio)
				m_Audio->Play(Sfx::UiClick);
		};
		if (interactive)
		{
			services.OnOptionsChanged = [this](const OptionsState& state) { ApplyOptions(state, true); };
			services.OnOptionsClosed = [this](const OptionsState& state) { (void)OptionsToSettings(state, m_SettingsBase).Save(); };
		}
		m_Screens->SetServices(std::move(services));
		m_Build->SetInteractHandler([this](Landmark landmark)
		{
			const bool opened = m_Screens->OpenLandmark(landmark);
			if (opened && m_Audio && (landmark == Landmark::Shop || landmark == Landmark::ShippingBin))
			{
				const LandmarkSpec& spec = GetLandmarkSpec(landmark);
				m_Audio->PlayAt(landmark == Landmark::Shop ? Sfx::ShopBell : Sfx::BinThunk,
					glm::vec3((static_cast<float>(spec.DoorX) + 0.5f) * TILE_SIZE, GROUND_TOP, (static_cast<float>(spec.DoorY) + 0.5f) * TILE_SIZE));
			}
			return opened;
		});
		m_Build->SetTileInteractHandler([this](int x, int y)
		{
			const PlaceInteraction interaction = InteractWithPlace(m_Director->GetGrid(), m_State, x, y);
			if (interaction.Handled)
				m_Screens->Say(interaction.Message);
			return interaction.Handled;
		});
		m_Screens->SetVillagerLines([this](VillagerId villager) { return m_Dialogue->Talk(villager); });
		m_Screens->SetVillagerGift([this](VillagerId villager) { return m_Dialogue->Gift(villager); });

		if (interactive)
		{
			const OptionsState saved = OptionsFromSettings(m_SettingsBase);
			m_Screens->SetOptions(saved);
			ApplyOptions(saved, true);
			if (!app.IsHeadless() && m_Player && !m_LoadedAtLaunch)
				m_Screens->OpenTitle();
		}
	}

	void HearthvaleLayer::ApplyOptions(const OptionsState& state, bool toEngine)
	{
		if (m_Camera)
			m_Camera->SetSpeedScale(state.CameraSpeed);
		if (m_Build)
			m_Build->SetHoldToPlace(state.HoldToPlace);
		if (toEngine)
			OptionsToSettings(state, m_SettingsBase).Apply();
	}

	void HearthvaleLayer::OnDetach()
	{
		m_StaticChecks.reset();
		m_LifeChecks.reset();
		m_UiChecks.reset();
		ReleaseScenePieces();
		m_World.reset();
		if (Scene* active = m_SceneManager.GetActiveScene())
			active->OnStop();
		m_SceneManager.Clear();
		m_FarmerAssets.reset();
		m_ValleyScene = nullptr;
		m_Ghost.reset();
		m_Screens.reset();
		m_Saves.reset();
		m_Build.reset();
		m_Sleep.reset();
		m_Dialogue.reset();
		m_AnimalSim.reset();
		m_Villagers.reset();
		m_Economy.reset();
		m_Farm.reset();
		m_Director.reset();
	}

	void HearthvaleLayer::ReleaseScenePieces()
	{
		if (m_Ghost)
			m_Ghost->Release();
		m_Fade.reset();
		m_Lights.reset();
		m_Vfx.reset();
		m_Audio.reset();
		m_HasLastFeet = false;
		m_Ui.reset();
		m_Camera.reset();
		m_AnimalWorld.reset();
		m_VillagerWorld.reset();
		m_Player.reset();
		m_Sky.reset();
	}

	void HearthvaleLayer::RegisterStages()
	{
		m_Director->OnStage(SimStage::PlayerIntent, [this](ValleyDirector& director, const StepInfo& step)
		{
			if (m_Player)
				m_Player->Step(ActiveIntent(), director.GetGrid(), m_Camera ? m_Camera->GetYaw() : 0.0f, step.Dt);
		});
		m_Director->OnStage(SimStage::ToolsAndPlacement, [this](ValleyDirector& director, const StepInfo&)
		{
			if (!m_Player || !m_Build)
				return;
			m_Aim.Feet = glm::vec2(m_Player->GetFeet().x, m_Player->GetFeet().z);
			m_Aim.Yaw = m_Player->GetYaw();
			const PlayerIntent& intent = ActiveIntent();
			if (intent.Move != glm::vec2(0.0f))
				m_Aim.HasMouse = false;
			PlayerIntent stepIntent = intent;
			if (m_Build->GetMode() == BuildMode::Tools && (intent.Interact || intent.Use))
			{
				if (const std::optional<VillagerId> talk = FindTalkTarget(intent))
				{
					const glm::vec2 toVillager = m_Villagers->GetPlace(*talk).Position - m_Aim.Feet;
					if (glm::length(toVillager) > 0.01f)
						m_Player->SetYaw(PlayerMath::YawOf(toVillager));
					if (m_Screens->OpenVillager(*talk))
						stepIntent.Interact = stepIntent.Use = false;
				}
			}
			m_Build->Step(stepIntent, m_Aim);
			if (m_Build->GetAim().FromMouse && intent.Move == glm::vec2(0.0f))
				m_Player->SetYaw(m_Build->GetAim().FaceYaw);
		});
		m_Director->OnStage(SimStage::Sky, [this](ValleyDirector& director, const StepInfo& step)
		{
			if (m_Sky)
				m_Sky->Update(step.Dt, director.GetCalendar().GetHours(), director.GetCalendar().GetWeather());
		});
		m_Director->OnStage(SimStage::VfxAudio, [this](ValleyDirector& director, const StepInfo& step)
		{
			UpdateVfxAudio(director, step.Dt);
		});
		m_Director->OnStage(SimStage::Camera, [this](ValleyDirector&, const StepInfo& step)
		{
			if (m_Camera && m_Player)
				{
				const PlayerIntent& intent = ActiveIntent();
				m_Camera->Update(step.Dt, m_Player->GetFeet(), intent.CameraSteps, intent.CameraZoomAxis, intent.CameraZoomWheel);
			}
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
		m_Ghost.reset();
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
		m_VillagerWorld = std::make_unique<VillagerWorld>(*m_ValleyScene);
		if (ANIMALS_ENABLED)
			m_AnimalWorld = std::make_unique<AnimalWorld>(*m_ValleyScene);
		m_Ghost = std::make_unique<BuildGhost>(*m_ValleyScene);
		m_Ui = std::make_unique<GameUi>(*m_Director, m_State.Pack, *m_Build, *m_Screens);
		m_Fade = std::make_unique<FadeOverlay>();

		m_Vfx = std::make_unique<Vfx>(*m_ValleyScene, !options.NoParticles);
		m_Lights = std::make_unique<NightLights>(*m_ValleyScene, m_Director->GetGrid(), *m_Vfx);
		m_Audio = std::make_unique<GameAudio>(calendar.GetSeason());
		m_LightsSeenCells = ~0ull;
		m_LastCoins = m_State.Pack.GetCoins();
		m_LastSlot = m_State.Pack.GetSelectedSlot();
		m_ScreenWasOpen = false;
	}

	void HearthvaleLayer::UpdateVfxAudio(const ValleyDirector& director, float dt)
	{
		const Calendar& calendar = director.GetCalendar();
		m_WeatherFx.SetWeather(calendar.GetWeather());
		if (!m_Camera || !m_Player)
			return;

		const float hours = calendar.GetHours();
		const Season season = calendar.GetSeason();
		const Weather weather = m_WeatherFx.GetWeather();
		const glm::vec3 focus = m_Camera->GetFocus();

		if (m_Lights)
		{
			if (m_World && m_World->GetCellsRebuiltTotal() != m_LightsSeenCells)
			{
				m_LightsSeenCells = m_World->GetCellsRebuiltTotal();
				m_Lights->Rescan();
			}
			const Window& window = Application::Get().GetWindow();
			const float aspect = static_cast<float>(window.GetWidth()) / (std::max)(1.0f, static_cast<float>(window.GetHeight()));
			m_Lights->Update(dt, NightCurve::LampAmount(hours, weather), NightCurve::ChimneyAmount(hours, season),
				m_ValleyScene->GetActiveCameraViewProjection(aspect), focus);
		}

		if (m_Vfx)
		{
			VfxFrame frame;
			frame.Focus = focus;
			frame.WeatherNow = weather;
			frame.FireflyAmount = NightCurve::FireflyAmount(hours, season, weather);
			frame.MistAmount = NightCurve::MistAmount(hours, season, weather);
			m_Vfx->Update(frame);
		}

		if (m_Audio)
		{
			const glm::vec3 feet = m_Player->GetFeet();
			const float moved = m_HasLastFeet ? glm::length(glm::vec2(feet.x - m_LastFeet.x, feet.z - m_LastFeet.z)) : 0.0f;
			m_LastFeet = feet;
			m_HasLastFeet = true;

			AudioFrame& frame = m_AudioFrame;
			frame.Dt = dt;
			frame.Hours = hours;
			frame.SeasonNow = season;
			frame.WeatherNow = weather;
			frame.Listener = m_Camera->GetEntity().GetComponent<Transform3DComponent>().Position;
			frame.Feet = feet;
			frame.Walked = moved < FOOTSTEP_MAX_JUMP ? moved : 0.0f;
			const TileGrid& grid = director.GetGrid();
			const int tileX = static_cast<int>(std::floor(feet.x / TILE_SIZE));
			const int tileY = static_cast<int>(std::floor(feet.z / TILE_SIZE));
			frame.Under = grid.InBounds(tileX, tileY) ? grid.GetGround(tileX, tileY) : Ground::Grass;
			frame.Chickens.clear();
			if (m_AnimalSim)
			{
				for (const AnimalMotion& motion : m_AnimalSim->GetMotions())
				{
					if (motion.Kind == AnimalKind::Chicken)
						frame.Chickens.emplace_back(motion.Position.x * TILE_SIZE, GROUND_TOP, motion.Position.y * TILE_SIZE);
				}
			}
			m_Audio->Update(frame);
		}
	}

	void HearthvaleLayer::OnActionFeedback(ActionFeedback feedback, int x, int y)
	{
		if (!m_Audio && !m_Vfx)
			return;
		const glm::vec3 at((static_cast<float>(x) + 0.5f) * TILE_SIZE, GROUND_TOP, (static_cast<float>(y) + 0.5f) * TILE_SIZE);
		const auto sound = [&](Sfx sfx)
		{
			if (m_Audio)
				m_Audio->PlayAt(sfx, at);
		};
		const auto burst = [&](VfxBurst kind)
		{
			if (m_Vfx)
				m_Vfx->Burst(kind, at);
		};
		switch (feedback)
		{
			case ActionFeedback::Till:    sound(Sfx::ToolHoe); burst(VfxBurst::Dust); break;
			case ActionFeedback::Water:   sound(Sfx::ToolWater); burst(VfxBurst::Spray); break;
			case ActionFeedback::Plant:   sound(Sfx::Plant); break;
			case ActionFeedback::Harvest: sound(Sfx::Harvest); burst(VfxBurst::Harvest); break;
			case ActionFeedback::Chop:    sound(Sfx::ToolAxe); burst(VfxBurst::Chips); break;
			case ActionFeedback::Mine:    sound(Sfx::ToolPick); burst(VfxBurst::Dust); break;
			case ActionFeedback::Cut:     sound(Sfx::ToolScythe); break;
			case ActionFeedback::Place:   sound(Sfx::Place); burst(VfxBurst::Dust); break;
			case ActionFeedback::Remove:  sound(Sfx::Remove); burst(VfxBurst::Dust); break;
			case ActionFeedback::Denied:
				if (m_Audio)
					m_Audio->Play(Sfx::UiError);
				break;
		}
	}

	void HearthvaleLayer::PollUiSounds()
	{
		if (!m_Audio || !m_Screens)
			return;
		const bool open = m_Screens->IsOpen();
		if (open != m_ScreenWasOpen)
			m_Audio->Play(open ? Sfx::UiOpen : Sfx::UiClose);
		m_ScreenWasOpen = open;

		const int coins = m_State.Pack.GetCoins();
		if (coins > m_LastCoins)
			m_Audio->Play(Sfx::UiCoin);
		m_LastCoins = coins;

		const int slot = m_State.Pack.GetSelectedSlot();
		if (slot != m_LastSlot)
			m_Audio->Play(Sfx::UiFocus);
		m_LastSlot = slot;
	}

	const PlayerIntent& HearthvaleLayer::ActiveIntent() const
	{
		const bool blocked = (m_Sleep && m_Sleep->BlocksInput()) || (m_Screens && m_Screens->IsOpen());
		return blocked ? m_NoIntent : m_Intent;
	}

	void HearthvaleLayer::UpdateMouseAim()
	{
		const glm::vec2 mouse = Input::GetMousePosition();
		const bool moved = m_MouseSeen && mouse != m_LastMouse;
		m_LastMouse = mouse;
		m_MouseSeen = true;
		if (!moved && !m_Aim.HasMouse)
			return;

		const Window& window = Application::Get().GetWindow();
		m_MouseRay = m_ValleyScene->ScreenPointToRay(mouse, glm::vec2(static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight())));
		glm::vec3 hit(0.0f);
		m_Aim.HasMouse = m_MouseRay.IntersectGroundPlane(GROUND_TOP, hit);
		m_Aim.MouseGround = glm::vec2(hit.x, hit.z);
	}

	std::optional<VillagerId> HearthvaleLayer::FindTalkTarget(const PlayerIntent& intent) const
	{
		const VillagerPlaces& places = m_Villagers->GetPlaces();
		if (m_Aim.HasMouse && m_VillagerWorld)
		{
			const std::optional<VillagerId> hit = m_VillagerWorld->Pick(m_MouseRay, VILLAGER_PICK_RAY_LENGTH);
			if (hit && glm::length(places[static_cast<size_t>(*hit)].Position - m_Aim.Feet) <= VILLAGER_PICK_REACH)
				return hit;
		}
		if (!intent.Interact)
			return std::nullopt;
		const std::optional<VillagerPick> near = PickNearby(places, m_Aim.Feet, m_Aim.Yaw, VILLAGER_TALK_RADIUS, VILLAGER_TALK_ARC_DEG);
		if (!near)
			return std::nullopt;
		return near->Who;
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
		if (!m_LifeChecksStarted)
		{
			m_LifeChecksStarted = true;
			if (!Application::Get().IsHeadless() && m_VillagerWorld)
				m_LifeChecks = std::make_unique<LifeWorldCheckRun>();
		}
		if (m_LifeChecks && !m_LifeChecks->Tick(m_Report, *m_Director, *m_Villagers, m_VillagerWorld.get(), m_AnimalWorld.get(), m_State.Animals))
			return;

		m_LifeChecks.reset();
		if (!m_UiChecksStarted)
		{
			m_UiChecksStarted = true;
			if (!Application::Get().IsHeadless() && m_Ui && m_Screens && !GetLaunchOptions().Autoplay)
				m_UiChecks = std::make_unique<UiCheckRun>();
		}
		if (m_UiChecks && !m_UiChecks->Tick(m_Report, *m_Ui, *m_Screens, m_State.Pack, *m_Build, *m_Director))
			return;

		m_UiChecks.reset();
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
		{
			PollPlayerIntent(m_Intent);
			if (m_Screens)
				m_Screens->Update(m_Intent, m_Sleep && m_Sleep->BlocksInput());
			UpdateMouseAim();
		}

		const float step = options.FixedDt > 0.0f ? options.FixedDt : deltaTime;
		const int steps = options.FixedDt > 0.0f ? options.StepsPerFrame : 1;
		if (m_Saves && !(m_Screens && m_Screens->IsOpen(ScreenId::Title)))
			m_Saves->AddPlayTime(step * static_cast<float>(steps));
		for (int i = 0; i < steps; ++i)
		{
			m_Director->Advance(step, SIM_MAX_STEPS_REAL_TIME);
			m_SceneManager.OnUpdate(step);
		}
		if (m_World)
			m_World->Sync();
		if (m_VillagerWorld)
			m_VillagerWorld->Sync(m_Villagers->GetPlaces(), step);
		if (m_AnimalWorld)
			m_AnimalWorld->Sync(m_State.Animals, *m_AnimalSim, step);
		PollUiSounds();
		if (m_Ui)
			m_Ui->Draw(deltaTime);
		if (m_Ghost && m_Build)
			m_Ghost->Update(*m_Build, m_Director->GetGrid());
		if (m_Fade && m_Sleep)
			m_Fade->Draw(m_Sleep->GetAlpha());
		m_SceneManager.OnRender();
	}

}
