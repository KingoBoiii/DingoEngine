#include "depch.h"
#include "DingoEngine/Asset/AssetManager.h"

#include "DingoEngine/Asset/AssetManagerData.h"
#include "DingoEngine/Asset/Pack/PackReader.h"
#include "DingoEngine/Asset/Pack/PackWriter.h"
#include "DingoEngine/Core/IO.h"
#include "DingoEngine/Core/VirtualFileSystem.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <random>

namespace Dingo
{

	namespace
	{

		std::filesystem::path FromUtf8(const std::string& text)
		{
			return std::filesystem::path(std::u8string(text.begin(), text.end()));
		}

		std::string ToUtf8(const std::filesystem::path& path)
		{
			const std::u8string text = path.generic_u8string();
			return std::string(text.begin(), text.end());
		}

		int HexDigit(char c)
		{
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			return -1;
		}

		bool GlobMatch(std::string_view pattern, std::string_view text)
		{
			if (pattern.empty())
				return text.empty();

			if (pattern.starts_with("**"))
			{
				std::string_view rest = pattern.substr(2);
				if (rest.starts_with('/'))
				{
					if (GlobMatch(rest.substr(1), text))
						return true;
				}
				for (size_t i = 0; i <= text.size(); ++i)
				{
					if (GlobMatch(rest, text.substr(i)))
						return true;
				}
				return false;
			}

			if (pattern[0] == '*')
			{
				for (size_t i = 0; i <= text.size(); ++i)
				{
					if (GlobMatch(pattern.substr(1), text.substr(i)))
						return true;
					if (i < text.size() && text[i] == '/')
						break;
				}
				return false;
			}

			if (text.empty())
				return false;
			if (pattern[0] == '?' ? text[0] != '/' : pattern[0] == text[0])
				return GlobMatch(pattern.substr(1), text.substr(1));
			return false;
		}

		bool IsExcluded(const std::string& relative, const std::vector<std::string>& patterns)
		{
			const size_t slash = relative.rfind('/');
			const std::string_view fileName = slash == std::string::npos ? std::string_view(relative) : std::string_view(relative).substr(slash + 1);

			for (std::string pattern : patterns)
			{
				const bool directory = pattern.ends_with('/');
				if (directory)
					pattern.pop_back();
				if (pattern.starts_with('/'))
					pattern.erase(0, 1);
				if (pattern.empty())
					continue;
				const bool anchored = pattern.find('/') != std::string::npos;

				if (!directory)
				{
					if (anchored ? GlobMatch(pattern, relative) : GlobMatch(pattern, fileName))
						return true;
					continue;
				}

				for (size_t end = relative.find('/'); end != std::string::npos; end = relative.find('/', end + 1))
				{
					const std::string_view folder = std::string_view(relative).substr(0, end);
					const size_t start = folder.rfind('/');
					const std::string_view folderName = start == std::string_view::npos ? folder : folder.substr(start + 1);
					if (anchored ? GlobMatch(pattern, folder) : GlobMatch(pattern, folderName))
						return true;
				}
			}
			return false;
		}

		std::vector<std::string> ReadIgnoreFile(const std::filesystem::path& file)
		{
			std::vector<std::string> patterns;
			std::ifstream stream(file);
			std::string line;
			while (std::getline(stream, line))
			{
				while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back())))
					line.pop_back();
				const size_t start = line.find_first_not_of(" \t");
				if (start == std::string::npos || line[start] == '#')
					continue;
				patterns.push_back(line.substr(start));
			}
			return patterns;
		}

		void ReloadIfLoaded(AssetManager& manager, AssetHandle handle)
		{
			if (manager.GetState(handle) == AssetState::Ready)
				manager.Reload(handle);
		}

	}

	bool PackageKey::IsNull() const
	{
		return std::all_of(Bytes.begin(), Bytes.end(), [](uint8_t b) { return b == 0; });
	}

	std::string PackageKey::ToHex() const
	{
		static constexpr char k_Digits[] = "0123456789abcdef";
		std::string hex;
		hex.reserve(64);
		for (uint8_t b : Bytes)
		{
			hex.push_back(k_Digits[b >> 4]);
			hex.push_back(k_Digits[b & 15]);
		}
		return hex;
	}

	std::optional<PackageKey> PackageKey::FromHex(std::string_view hex)
	{
		PackageKey key;
		size_t digits = 0;
		for (char c : hex)
		{
			if (std::isspace(static_cast<unsigned char>(c)))
				continue;
			const int value = HexDigit(c);
			if (value < 0 || digits >= 64)
				return std::nullopt;
			key.Bytes[digits / 2] = static_cast<uint8_t>(key.Bytes[digits / 2] | (digits % 2 == 0 ? value << 4 : value));
			++digits;
		}
		if (digits != 64)
			return std::nullopt;
		return key;
	}

	std::optional<PackageKey> PackageKey::FromFile(const std::filesystem::path& path)
	{
		std::ifstream stream(path);
		if (!stream)
			return std::nullopt;

		std::string hex, line;
		while (std::getline(stream, line))
		{
			const size_t start = line.find_first_not_of(" \t\r");
			if (start != std::string::npos && line[start] != '#')
				hex += line;
		}
		return FromHex(hex);
	}

	PackageKey PackageKey::Generate()
	{
		std::random_device random;
		PackageKey key;
		for (size_t i = 0; i < key.Bytes.size(); i += 4)
		{
			const uint32_t value = random();
			std::memcpy(key.Bytes.data() + i, &value, 4);
		}
		return key;
	}

	PackageResult AssetManager::Package(const PackageParams& params)
	{
		PackageResult result;
		const std::filesystem::path source = params.SourceDirectory.empty() ? VirtualFileSystem::GetRoot() : params.SourceDirectory;
		std::error_code ec;
		if (source.empty() || !std::filesystem::is_directory(source, ec))
		{
			result.Error = source.empty() ? "no source directory, and no AssetManager root to default to" : "'" + source.string() + "' is not a directory";
			return result;
		}
		if (params.OutputPath.empty())
		{
			result.Error = "no output path";
			return result;
		}

		std::vector<std::string> patterns = params.Exclude;
		const std::filesystem::path ignoreFile = source / ".dpakignore";
		for (std::string& pattern : ReadIgnoreFile(ignoreFile))
			patterns.push_back(std::move(pattern));

		const std::filesystem::path output = std::filesystem::absolute(params.OutputPath).lexically_normal();
		std::vector<std::pair<std::string, std::filesystem::path>> files;
		for (auto it = std::filesystem::recursive_directory_iterator(source, std::filesystem::directory_options::skip_permission_denied, ec);
			!ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
		{
			if (!it->is_regular_file(ec))
				continue;

			const std::filesystem::path file = std::filesystem::absolute(it->path()).lexically_normal();
			const std::string relative = ToUtf8(it->path().lexically_relative(source));
			if (relative == ".dpakignore" || IsExcluded(relative, patterns))
				continue;
			if (file == output)
			{
				result.Warnings.push_back("skipped '" + relative + "', the output itself");
				continue;
			}
			files.emplace_back(relative, file);
		}
		if (ec)
		{
			result.Error = "can't walk '" + source.string() + "': " + ec.message();
			return result;
		}
		if (files.empty())
		{
			result.Error = "no files to package under '" + source.string() + "'";
			return result;
		}

		std::sort(files.begin(), files.end());
		std::unordered_map<std::string, std::string> folded;
		for (const auto& [relative, file] : files)
		{
			auto [it, inserted] = folded.emplace(Internal::Pack::FoldCase(relative), relative);
			if (!inserted)
			{
				result.Error = "'" + it->second + "' and '" + relative + "' differ only by case, which a package can't tell apart";
				return result;
			}
		}

		Internal::PackWriter writer(params.Key);
		for (const auto& [relative, file] : files)
		{
			std::ifstream stream(file, std::ios::binary);
			std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
			if (!stream && !stream.eof())
			{
				result.Error = "can't read '" + relative + "'";
				return result;
			}
			if (!writer.Add(relative, bytes, result.Error))
				return result;
		}

		const std::string archive = writer.Finish();
		if (!IO::WriteFileAtomic(output, archive))
		{
			result.Error = "can't write '" + output.string() + "'";
			return result;
		}

		result.Success = true;
		result.FileCount = writer.GetEntryCount();
		result.RawBytes = writer.GetRawBytes();
		result.PackedBytes = archive.size();
		return result;
	}

	PackageHandle AssetManager::Unpackage(const std::filesystem::path& dpak, const PackageKey& key)
	{
		Internal::AssetManagerData& data = *m_Data;

		std::string error;
		std::shared_ptr<Internal::PackReader> reader = Internal::PackReader::Open(dpak, key, error);
		if (!reader)
		{
			DE_CORE_ERROR("AssetManager: can't open package '{}': {}.", dpak.string(), error);
			return k_InvalidPackage;
		}

		Internal::MountedPackage package;
		package.Name = reader->GetName();
		package.Mount = Internal::MountArchive(reader);

		for (uint32_t i = 0; i < reader->GetEntryCount(); ++i)
		{
			const std::filesystem::path entry = FromUtf8(reader->GetEntry(i).Name);
			if (AssetTypeFromExtension(entry.extension()) == AssetType::None)
				continue;

			const AssetHandle existing = FindByPath(entry);
			const AssetHandle handle = IsValidAssetHandle(existing) ? existing : Import(entry);
			if (!IsValidAssetHandle(handle))
				continue;

			package.Assets.push_back(handle);
			if (!IsValidAssetHandle(existing))
				package.Created.insert(handle);
			else
				ReloadIfLoaded(*this, handle);
		}

		PackageHandle handle = PackageHandle::Generate();
		while (data.Packages.contains(handle))
			handle = PackageHandle::Generate();

		DE_CORE_INFO("AssetManager: mounted package '{}' ({} files, {} assets{}).", package.Name, reader->GetEntryCount(), package.Assets.size(),
			reader->IsEncrypted() ? ", encrypted" : "");
		data.Packages.emplace(handle, std::move(package));
		return handle;
	}

	std::span<const AssetHandle> AssetManager::GetPackageAssets(PackageHandle package) const
	{
		auto it = m_Data->Packages.find(package);
		if (it == m_Data->Packages.end())
			return {};
		return it->second.Assets;
	}

	bool AssetManager::IsPackageMounted(PackageHandle package) const
	{
		return m_Data->Packages.contains(package);
	}

	void AssetManager::ClosePackage(PackageHandle handle)
	{
		Internal::AssetManagerData& data = *m_Data;
		auto it = data.Packages.find(handle);
		if (it == data.Packages.end())
			return;

		Internal::MountedPackage package = std::move(it->second);
		data.Packages.erase(it);
		VirtualFileSystem::Unmount(package.Mount);

		for (AssetHandle asset : package.Assets)
		{
			if (!data.Registry.contains(asset))
				continue;

			Internal::MountedPackage* heir = nullptr;
			for (auto& [other, mounted] : data.Packages)
			{
				if (std::find(mounted.Assets.begin(), mounted.Assets.end(), asset) != mounted.Assets.end())
				{
					heir = &mounted;
					break;
				}
			}

			if (package.Created.contains(asset) && !heir)
			{
				Remove(asset);
				continue;
			}
			if (package.Created.contains(asset))
				heir->Created.insert(asset);
			ReloadIfLoaded(*this, asset);
		}

		DE_CORE_INFO("AssetManager: closed package '{}'.", package.Name);
	}

	std::string AssetManager::GetAssetSource(AssetHandle handle) const
	{
		const AssetMetadata* metadata = GetMetadata(handle);
		if (!metadata)
			return {};

		std::optional<VfsResolvedPath> resolved = VirtualFileSystem::Resolve(metadata->AbsolutePath);
		if (!resolved)
			return "missing";
		if (!resolved->DiskPath.empty())
			return "loose";
		return VirtualFileSystem::GetMountName(resolved->Mount);
	}

}
