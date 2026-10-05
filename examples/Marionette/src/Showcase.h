#pragma once
#include <DingoEngine.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace Dingo
{

	class ArenaWorld;
	class CameraRig;
	class Fighter;
	class GameAssets;
	class GameAudio;
	class HitDebugView;

	enum class ShowcaseKind : uint8_t
	{
		Lineup,
		Title,
		Victory
	};

	struct ShowcaseParams
	{
		ShowcaseKind Kind = ShowcaseKind::Lineup;
		// Lineup: hold the idle frame.
		bool Freeze = false;
		// Lineup: the camera over the arena.
		bool Overview = false;
		// Victory: the Knight taunts; otherwise it idles.
		bool Taunt = true;
		const HitDebugView* Debug = nullptr;
	};

	// The arena with fighters standing in it and a camera on them, and nothing to play: the four-fighter --lineup,
	// the title's row of the same four, and the Knight taunting in front of the camera on the end screen.
	class Showcase
	{
	public:
		Showcase(Scene& scene, const GameAssets& assets, const ShowcaseParams& params);
		~Showcase();

		Showcase(const Showcase&) = delete;
		Showcase& operator=(const Showcase&) = delete;

		void Update(float deltaTime);

	private:
		void BuildRow(Scene& scene, const GameAssets& assets, const ShowcaseParams& params);
		void BuildVictory(Scene& scene, const GameAssets& assets, const ShowcaseParams& params);

	private:
		const GameAssets& m_Assets;
		uint32_t m_EventGeneration = 0;
		double m_Time = 0.0;
		std::unique_ptr<GameAudio> m_Audio;
		std::unique_ptr<ArenaWorld> m_World;
		std::vector<std::unique_ptr<Fighter>> m_Fighters;
		std::unique_ptr<CameraRig> m_Camera;
	};

}
