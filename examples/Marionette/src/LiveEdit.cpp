#include "LiveEdit.h"
#include "GameAssets.h"
#include "GameTuning.h"
#include "Moveset.h"

#include <DingoEngine.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
	using namespace Dingo;

	constexpr const char* k_Folder = "MarionetteLiveEdit";
	constexpr const char* k_Library = "CombatMelee";
	constexpr const char* k_Clip = "Melee_1H_Attack_Slice_Diagonal";

	bool s_Prepared = false;

	bool Within(const std::filesystem::path& path, const std::filesystem::path& root)
	{
		std::error_code error;
		const std::filesystem::path full = std::filesystem::weakly_canonical(path, error);
		if (error)
			return false;
		const std::filesystem::path base = std::filesystem::weakly_canonical(root, error);
		if (error)
			return false;

		const std::filesystem::path relative = full.lexically_relative(base);
		return !relative.empty() && *relative.begin() != "..";
	}

	bool Overlaps(const std::filesystem::path& a, const std::filesystem::path& b)
	{
		std::error_code error;
		const std::filesystem::path first = std::filesystem::weakly_canonical(a, error);
		if (error)
			return true;
		const std::filesystem::path second = std::filesystem::weakly_canonical(b, error);
		if (error)
			return true;
		return Within(first, second) || Within(second, first);
	}

	bool IsLink(const std::filesystem::path& path)
	{
		std::error_code error;
		const std::filesystem::file_status status = std::filesystem::symlink_status(path, error);
		return !error && (std::filesystem::is_symlink(status) || status.type() == std::filesystem::file_type::junction);
	}

	bool IsPlain(const std::filesystem::path& path)
	{
		std::error_code error;
		const std::filesystem::path resolved = std::filesystem::weakly_canonical(path, error);
		if (error)
			return false;
		const std::filesystem::path parent = std::filesystem::weakly_canonical(path.parent_path(), error);
		return !error && !IsLink(path) && resolved == parent / path.filename();
	}

	std::optional<ClipRange> HitboxOf(const Model& library)
	{
		const AnimationClip* clip = library.FindAnimation(k_Clip);
		return clip ? FindRange(*clip, Events::HITBOX) : std::nullopt;
	}

	bool ReplaceRange(std::string& text, std::string_view clip, std::string_view event, const std::string& range)
	{
		for (size_t start = 0; start < text.size();)
		{
			size_t stop = text.find('\n', start);
			if (stop == std::string::npos)
				stop = text.size();

			std::istringstream words(text.substr(start, stop - start));
			std::string lineClip;
			std::string current;
			std::string lineEvent;
			if (words >> lineClip >> current >> lineEvent && lineClip == clip && lineEvent == event && current.find("..") != std::string::npos)
			{
				text.replace(text.find(current, start), current.size(), range);
				return true;
			}
			start = stop + 1;
		}
		return false;
	}
}

namespace Dingo
{

	std::filesystem::path GetLiveEditRoot()
	{
		std::error_code error;
		const std::filesystem::path temp = std::filesystem::temp_directory_path(error);
		return error ? std::filesystem::path() : temp / k_Folder / "assets";
	}

	bool PrepareLiveEditAssets(const std::filesystem::path& source)
	{
		const std::filesystem::path root = GetLiveEditRoot();
		if (root.empty() || !root.is_absolute() || root.filename() != "assets" || root.parent_path().filename() != k_Folder)
		{
			DE_ERROR("[LiveEdit] no absolute temp directory to copy the assets into");
			return false;
		}

		if (!IsPlain(root.parent_path()) || !IsPlain(root))
		{
			DE_ERROR("[LiveEdit] '{}' or '{}' is a link, or cannot be resolved; refusing to delete through it", root.parent_path().string(), root.string());
			return false;
		}

		std::error_code error;
		if (!std::filesystem::is_directory(source, error) || Overlaps(root, source))
		{
			DE_ERROR("[LiveEdit] '{}' is not a directory apart from the copy '{}' (neither may contain the other); refusing to replace the copy", source.string(), root.string());
			return false;
		}

		std::filesystem::remove_all(root, error);
		if (!error)
			std::filesystem::create_directories(root, error);
		if (!error)
			std::filesystem::copy(source, root, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, error);
		if (error)
		{
			DE_ERROR("[LiveEdit] couldn't copy '{}' to '{}': {}", source.string(), root.string(), error.message());
			return false;
		}

		s_Prepared = true;
		DE_INFO("[LiveEdit] reading the assets from the copy in '{}'; the repository's own are never written", root.string());
		return true;
	}

	LiveEditDemo::LiveEditDemo(const GameAssets& assets)
		: m_Assets(assets)
	{}

	void LiveEditDemo::Update(float deltaTime)
	{
		if (m_Stage == Stage::Written)
		{
			const float waited = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_WrittenAt).count();
			if (waited >= LIVE_EDIT_RELOAD_TIMEOUT)
			{
				m_Stage = Stage::Done;
				DE_ERROR("[LiveEdit] no reload seen {:.1f} s after the write; hot-reload is off, or the game reads another copy", waited);
			}
		}

		if (m_Stage != Stage::Waiting)
			return;

		m_Time += deltaTime;
		if (m_Time >= LIVE_EDIT_DELAY)
			Write();
	}

	void LiveEditDemo::Write()
	{
		m_Stage = Stage::Done;

		const std::span<const LibraryDef> definitions = GetLibraryDefs();
		const Model* library = nullptr;
		for (size_t i = 0; i < definitions.size() && i < m_Assets.GetLibraries().size(); ++i)
		{
			if (std::string_view(definitions[i].Label) == k_Library)
				library = m_Assets.GetLibraries()[i];
		}

		const AnimationClip* clip = library ? library->FindAnimation(k_Clip) : nullptr;
		const MoveDef* move = FindMove(k_Clip);
		const std::optional<ClipRange> hitbox = library ? HitboxOf(*library) : std::nullopt;
		if (!clip || !move || !hitbox)
		{
			DE_ERROR("[LiveEdit] {} has no hitbox range to move", k_Clip);
			return;
		}

		std::filesystem::path file = library->GetFilePath();
		file.replace_extension(".events");
		const std::filesystem::path live = GetLiveEditRoot();
		if (live.empty() || !Within(file, live) || !Within(Application::Get().GetAssetManager().GetRootDirectory(), live))
		{
			DE_ERROR("[LiveEdit] the game is not reading the copy in the temp directory ('{}'), so nothing is written", file.string());
			return;
		}

		const float limit = clip->GetDuration() - move->FadeOut;
		const float shift = std::min(LIVE_EDIT_SHIFT, limit - hitbox->End);
		if (shift < LIVE_EDIT_MIN_SHIFT)
		{
			DE_ERROR("[LiveEdit] the hitbox {:.2f}..{:.2f} cannot move later and still end before {:.2f} s, where {} starts returning", hitbox->Begin, hitbox->End, limit, k_Clip);
			return;
		}

		std::string text;
		{
			std::ifstream in(file, std::ios::binary);
			std::stringstream buffer;
			buffer << in.rdbuf();
			text = buffer.str();
		}

		const float begin = hitbox->Begin + shift;
		const float end = hitbox->End + shift;
		if (!ReplaceRange(text, k_Clip, Events::HITBOX, std::format("{:.2f}..{:.2f}", begin, end)))
		{
			DE_ERROR("[LiveEdit] {} has no {} {} line to rewrite", file.string(), k_Clip, Events::HITBOX);
			return;
		}

		// The combo window opens where the hitbox closes, so it moves with it, or the swing's chain would open early.
		const std::optional<ClipRange> combo = FindRange(*clip, Events::COMBO);
		float comboBegin = 0.0f;
		float comboEnd = 0.0f;
		bool movedCombo = false;
		if (combo)
		{
			comboBegin = std::min(combo->Begin + shift, limit);
			comboEnd = std::max(std::min(combo->End + shift, limit), comboBegin);
			movedCombo = ReplaceRange(text, k_Clip, Events::COMBO, std::format("{:.2f}..{:.2f}", comboBegin, comboEnd));
			if (!movedCombo)
			{
				DE_WARN("[LiveEdit] {} has no {} {} line to move with the hitbox", file.string(), k_Clip, Events::COMBO);
			}
		}

		std::filesystem::path scratch = file;
		scratch += ".tmp";
		std::error_code error;
		{
			std::ofstream out(scratch, std::ios::binary | std::ios::trunc);
			out << text;
			out.close();
			if (!out)
			{
				DE_ERROR("[LiveEdit] couldn't write '{}'", scratch.string());
				std::filesystem::remove(scratch, error);
				return;
			}
		}

		std::filesystem::rename(scratch, file, error);
		if (error)
		{
			DE_ERROR("[LiveEdit] couldn't replace '{}' with the edit: {}", file.string(), error.message());
			std::filesystem::remove(scratch, error);
			return;
		}

		m_Begin = begin;
		m_End = end;
		m_HasCombo = movedCombo;
		m_ComboBegin = comboBegin;
		m_ComboEnd = comboEnd;
		m_WrittenAt = std::chrono::steady_clock::now();
		m_Stage = Stage::Written;
		DE_INFO("[LiveEdit] wrote hitbox {:.2f}..{:.2f} -> {:.2f}..{:.2f}{} ({} in '{}')", hitbox->Begin, hitbox->End, begin, end,
			movedCombo ? std::format(", combo {:.2f}..{:.2f} -> {:.2f}..{:.2f}", combo->Begin, combo->End, comboBegin, comboEnd) : std::string(), k_Clip, file.string());
	}

	void LiveEditDemo::OnReload()
	{
		if (m_Stage != Stage::Written)
			return;

		m_Stage = Stage::Done;
		const float seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_WrittenAt).count();
		DE_INFO("[LiveEdit] reload seen after {:.2f} s", seconds);

		const std::span<const LibraryDef> definitions = GetLibraryDefs();
		for (size_t i = 0; i < definitions.size() && i < m_Assets.GetLibraries().size(); ++i)
		{
			const Model* library = m_Assets.GetLibraries()[i];
			if (std::string_view(definitions[i].Label) != k_Library || !library)
				continue;

			const std::optional<ClipRange> now = HitboxOf(*library);
			if (now && std::abs(now->Begin - m_Begin) < 1.0e-3f && std::abs(now->End - m_End) < 1.0e-3f)
			{
				DE_INFO("[LiveEdit] {} now reads hitbox {:.2f}..{:.2f}", k_Clip, now->Begin, now->End);
			}
			else
			{
				DE_ERROR("[LiveEdit] {} reads hitbox {} after the reload, not the {:.2f}..{:.2f} written", k_Clip,
					now ? std::format("{:.2f}..{:.2f}", now->Begin, now->End) : std::string("none"), m_Begin, m_End);
			}

			if (!m_HasCombo)
				continue;

			const AnimationClip* clip = library->FindAnimation(k_Clip);
			const std::optional<ClipRange> combo = clip ? FindRange(*clip, Events::COMBO) : std::nullopt;
			if (combo && std::abs(combo->Begin - m_ComboBegin) < 1.0e-3f && std::abs(combo->End - m_ComboEnd) < 1.0e-3f)
			{
				DE_INFO("[LiveEdit] {} now reads combo {:.2f}..{:.2f}", k_Clip, combo->Begin, combo->End);
			}
			else
			{
				DE_ERROR("[LiveEdit] {} reads combo {} after the reload, not the {:.2f}..{:.2f} written", k_Clip,
					combo ? std::format("{:.2f}..{:.2f}", combo->Begin, combo->End) : std::string("none"), m_ComboBegin, m_ComboEnd);
			}
		}
	}

	void CleanupLiveEditAssets()
	{
		if (!s_Prepared)
			return;
		s_Prepared = false;

		const std::filesystem::path root = GetLiveEditRoot();
		const std::filesystem::path folder = root.parent_path();
		if (root.empty() || !root.is_absolute() || root.filename() != "assets" || folder.filename() != k_Folder || !IsPlain(folder) || !IsPlain(root))
		{
			DE_ERROR("[LiveEdit] '{}' is not the folder this run copied the assets into; leaving it alone", folder.string());
			return;
		}

		std::error_code error;
		std::filesystem::remove_all(folder, error);
		if (error)
		{
			DE_WARN("[LiveEdit] couldn't remove the copy in '{}': {}", folder.string(), error.message());
		}
		else
		{
			DE_INFO("[LiveEdit] removed the copy in '{}'", folder.string());
		}
	}

}
