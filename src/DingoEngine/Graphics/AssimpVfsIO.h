#pragma once

#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Dingo::Internal
{

	// Lets assimp read through the VirtualFileSystem, so a model and the files it names (a glTF's
	// .bin, an OBJ's .mtl) can come from a package. Paths cross the interface as UTF-8.
	class AssimpVfsIOSystem final : public Assimp::IOSystem
	{
	public:
		bool Exists(const char* file) const override;
		char getOsSeparator() const override { return '/'; }
		Assimp::IOStream* Open(const char* file, const char* mode = "rb") override;
		void Close(Assimp::IOStream* stream) override;
		bool ComparePaths(const char* one, const char* second) const override;

		static std::string ToAssimpPath(const std::filesystem::path& path);

	private:
		std::filesystem::path FindFile(const char* file) const;

		// The FBX pivot pass reads the whole file a second time.
		std::string m_CachedKey;
		std::shared_ptr<const std::vector<uint8_t>> m_CachedBytes;
	};

}
