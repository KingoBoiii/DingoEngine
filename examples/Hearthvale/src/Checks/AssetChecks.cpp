#include "Checks/Checks.h"
#include "Audio/GameAudio.h"
#include "Farm/Animals.h"
#include "Look/ArtRegistry.h"
#include "Look/FarmerAssets.h"
#include "Ui/GameUi.h"

#include <filesystem>
#include <format>
#include <set>
#include <string>
#include <vector>

namespace Dingo
{

	namespace
	{
		bool HasArt(std::string_view id)
		{
			return !id.empty() && Art::Find(id) != nullptr;
		}

		bool EveryIdResolves()
		{
			bool ok = true;
			for (int look = 0; look < static_cast<int>(GroundLook::Count); ++look)
				ok = HasArt(Art::ForGround(static_cast<GroundLook>(look))) && ok;
			for (int kind = 0; kind < DECOR_KIND_COUNT; ++kind)
				ok = HasArt(Art::ForDecor(static_cast<DecorKind>(kind))) && ok;
			for (int build = 0; build < BUILD_COUNT; ++build)
				ok = HasArt(Art::ForBuild(static_cast<BuildId>(build))) && ok;
			for (int landmark = 0; landmark < LANDMARK_COUNT; ++landmark)
				ok = HasArt(Art::ForLandmark(static_cast<Landmark>(landmark))) && ok;
			for (int crop = 0; crop < CROP_COUNT; ++crop)
			{
				for (int stage = 0; stage < CROP_STAGES; ++stage)
					ok = HasArt(Art::ForCrop(static_cast<CropId>(crop), stage)) && ok;
			}
			for (int villager = 0; villager < VILLAGER_COUNT; ++villager)
				ok = HasArt(Art::ForVillager(static_cast<VillagerId>(villager))) && ok;
			ok = HasArt(Art::ForVillagerHead()) && ok;
			for (const AnimalKind kind : { AnimalKind::Chicken, AnimalKind::Cow })
			{
				ok = HasArt(Art::ForAnimal(kind)) && ok;
				ok = HasArt(Art::ForAnimalHead(kind)) && ok;
				ok = HasArt(Art::ForProduce(kind)) && ok;
			}
			for (const std::string_view id : { Art::FARMER, Art::FARMER_HEAD, Art::FARMER_FACE, Art::TARGET_TILE, Art::SOIL, Art::SOIL_WATERED, Art::LAMP_BULB, Art::WINDOW })
				ok = HasArt(id) && ok;
			return ok;
		}

		bool EntriesAreUnique()
		{
			std::set<std::string_view> seen;
			for (const ArtEntry& entry : Art::All())
			{
				if (entry.Id.empty() || !seen.insert(entry.Id).second)
					return false;
			}
			return true;
		}

		std::vector<std::string> AssetFiles()
		{
			std::vector<std::string> files;
			for (const char* path : GameAudioPaths())
				files.push_back(path);
			for (const char* path : FarmerAssetPaths())
				files.push_back(path);
			files.push_back(GameUiFontPath());
			for (const ArtEntry& entry : Art::All())
			{
				if (!entry.ModelPath.empty())
					files.emplace_back(entry.ModelPath);
			}
			return files;
		}

		std::vector<std::string> MissingFiles(const std::vector<std::string>& files, const std::filesystem::path& root)
		{
			std::vector<std::string> missing;
			for (const std::string& file : files)
			{
				std::error_code error;
				if (!std::filesystem::is_regular_file(root / FileSystem::PathFromUtf8(file), error))
					missing.push_back(file);
			}
			return missing;
		}

		void CheckLoads(CheckReport& report, Application& app)
		{
			AssetManager& assets = app.GetAssetManager();

			const FarmerAssets farmer;
			report.Check(farmer.IsUsable() && farmer.GetMaterial() != nullptr && farmer.GetModelHeight() > 0.0f,
				"Assets: the farmer's model, texture and idle and walk clips load and resolve on its skeleton");

			Font* font = assets.GetFont(assets.Load(FileSystem::PathFromUtf8(GameUiFontPath())));
			report.Check(font != nullptr, "Assets: the UI font loads");

			bool modelsLoad = true;
			for (const ArtEntry& entry : Art::All())
			{
				if (!entry.ModelPath.empty())
					modelsLoad = assets.GetModel(assets.Load(FileSystem::PathFromUtf8(entry.ModelPath))) != nullptr && modelsLoad;
			}
			report.Check(modelsLoad, "Assets: every art entry with a model path loads it");

			AudioEngine& audio = app.GetAudioEngine();
			if (!audio.IsValid())
			{
				DE_INFO("Assets: no audio device, so the sounds are only checked on disk");
				return;
			}
			std::vector<std::string> failed;
			for (const char* path : GameAudioPaths())
			{
				if (!audio.LoadClip(FileSystem::PathFromUtf8(path)))
					failed.emplace_back(path);
			}
			report.Check(failed.empty(), std::format("Assets: every sound loads ({} did not{}{})", failed.size(), failed.empty() ? "" : ", first ", failed.empty() ? "" : failed.front()));
		}
	}

	void RunAssetChecks(CheckReport& report)
	{
		SuiteScope scope(report, "Assets");

		report.Check(EveryIdResolves(), "Assets: every art id the game asks for resolves in the registry");
		report.Check(EntriesAreUnique(), "Assets: the art registry has no empty or repeated id");

		Application& app = Application::Get();
		const std::filesystem::path& root = app.GetAssetManager().GetRootDirectory();
		const std::vector<std::string> files = AssetFiles();
		const std::vector<std::string> missing = MissingFiles(files, root);
		report.Check(!files.empty() && missing.empty(),
			std::format("Assets: all {} model, clip, sound and font files exist on disk ({} missing{}{})", files.size(), missing.size(),
				missing.empty() ? "" : ", first ", missing.empty() ? "" : missing.front()));

		if (!app.IsHeadless())
			CheckLoads(report, app);
	}

}
