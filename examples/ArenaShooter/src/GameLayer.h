#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <memory>
#include <random>
#include <vector>

namespace Dingo
{

	enum class GameState
	{
		Loading,
		Ready,
		Playing,
		GameOver
	};

	struct Bullet
	{
		glm::vec2 Position;
		glm::vec2 Velocity;
		float Life;
	};

	struct Enemy
	{
		glm::vec2 Position;
		float Speed;
		float Health;
	};

	struct Explosion
	{
		glm::vec2 Position;
		float Timer;
		float Size;
	};

	class GameLayer : public Layer
	{
	public:
		GameLayer() : Layer("Arena Shooter Game Layer") {}
		virtual ~GameLayer() = default;

	public:
		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(float deltaTime) override;

	private:
		void UpdateLoading(Renderer2D& renderer);
		void OnAssetsReady();

		void UpdateReady(float deltaTime, Renderer2D& renderer);
		void UpdatePlaying(float deltaTime, Renderer2D& renderer);
		void UpdateGameOver(float deltaTime, Renderer2D& renderer);

		void StartGame();
		void StartNextWave();

		void RenderBackground();
		void RenderHUD(Renderer2D& renderer);
		void DrawCenteredText(Renderer2D& renderer, const std::string& text, float size, const glm::vec2& offset, const glm::vec4& color = glm::vec4(1.0f));

		glm::vec2 GetAimDirection() const;
		bool WantsStart() const;

		const uint32_t GetGameVersion() const { return DE_MAKE_VERSION(1, 0, 0); }

	private:
		GameState m_GameState = GameState::Loading;

		float m_Width = 0.0f;
		float m_Height = 0.0f;
		float m_Time = 0.0f;
		glm::mat4 m_Projection = glm::mat4(1.0f);

		AssetHandle m_PlayerTexHandle = k_InvalidAsset;
		AssetHandle m_EnemyTexHandle = k_InvalidAsset;
		AssetHandle m_BulletTexHandle = k_InvalidAsset;
		AssetHandle m_ShaderHandle = k_InvalidAsset;
		AssetHandle m_FontHandle = k_InvalidAsset;
		AssetHandle m_ShootClip = k_InvalidAsset;
		AssetHandle m_DeathClip = k_InvalidAsset;
		AssetHandle m_HitClip = k_InvalidAsset;
		AssetHandle m_WaveClip = k_InvalidAsset;
		uint32_t m_TotalAssets = 0;

		Texture* m_PlayerTexture = nullptr;
		Texture* m_EnemyTexture = nullptr;
		Texture* m_BulletTexture = nullptr;
		Font* m_Font = nullptr;

		Pipeline* m_BackgroundPipeline = nullptr;
		Shader* m_BackgroundShader = nullptr;
		GraphicsBuffer* m_BackgroundVertexBuffer = nullptr;
		GraphicsBuffer* m_BackgroundIndexBuffer = nullptr;
		GraphicsBuffer* m_BackgroundUniformBuffer = nullptr;

		glm::vec2 m_PlayerPosition = glm::vec2(0.0f);
		int32_t m_Lives = 3;
		float m_InvulnerabilityTimer = 0.0f;
		float m_FireCooldown = 0.0f;

		std::vector<Bullet> m_Bullets;
		std::vector<Enemy> m_Enemies;
		std::vector<Explosion> m_Explosions;

		int32_t m_Wave = 0;
		int32_t m_EnemiesToSpawn = 0;
		float m_SpawnTimer = 0.0f;
		float m_WaveBannerTimer = 0.0f;
		uint32_t m_Score = 0;

		std::mt19937 m_Rng{ std::random_device{}() };
	};

}
