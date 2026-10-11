#pragma once
#include "Tests/Renderer2D/Renderer2DTest.h"
#include "Tests/TestChecks.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Dingo
{

	// Checks ChaCha20 against RFC 8439, then packages test/assets into an encrypted .dpak in the
	// cache directory, mounts it over the loose files and checks that every file, and every kind of
	// asset built from one, reads back identically; then a patch package's override order,
	// ClosePackage's fallback to the loose files, and damaged, truncated and wrongly keyed packages
	// failing cleanly.
	class PackTest : public Renderer2DTest
	{
	public:
		PackTest(Renderer2D* renderer)
			: Renderer2DTest(renderer)
		{}
		virtual ~PackTest() = default;

	public:
		virtual void Initialize() override;
		virtual void Update(float deltaTime) override;
		virtual void Cleanup() override;
		virtual void ImGuiRender() override;

	private:
		struct ModelCounts
		{
			uint32_t SubMeshes = 0;
			uint64_t Vertices = 0;
			uint32_t Joints = 0;
			uint32_t Clips = 0;
			uint64_t Events = 0;
			bool operator==(const ModelCounts&) const = default;
		};

		struct Snapshot
		{
			ModelCounts Fox;
			ModelCounts Duck;
			ModelCounts DuckEmbedded;
			uint64_t ClipFrames = 0;
			float FontWidth = 0.0f;
			bool ShaderValid = false;
		};

		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }
		Snapshot TakeSnapshot();
		void CheckCipher();
		void CheckPackaging();
		void CheckEncryption();
		void CheckMountedBase();
		void CheckPatchAndClose();
		void CheckDamagedPackages();
		void FinishChecks();

	private:
		TestChecks m_Checks;
		std::filesystem::path m_WorkDirectory;
		std::filesystem::path m_BasePackage;
		std::filesystem::path m_PatchPackage;
		std::vector<std::string> m_Files;
		uint32_t m_AssetFileCount = 0;

		PackageKey m_Key;
		PackageHandle m_Base = k_InvalidPackage;
		Snapshot m_Loose;

		Texture* m_LooseTexture = nullptr;
		Texture* m_PackedTexture = nullptr;
		std::optional<TexturePixels> m_LoosePixels;
		std::optional<TexturePixels> m_PackedPixels;

		int m_Stage = 0;
		bool m_Done = false;
	};

}
