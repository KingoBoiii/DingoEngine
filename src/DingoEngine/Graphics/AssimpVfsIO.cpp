#include "depch.h"
#include "DingoEngine/Graphics/AssimpVfsIO.h"

#include "DingoEngine/Core/VirtualFileSystem.h"

#include <algorithm>
#include <cstring>

namespace Dingo::Internal
{

	namespace
	{

		class MemoryStream final : public Assimp::IOStream
		{
		public:
			explicit MemoryStream(std::shared_ptr<const std::vector<uint8_t>> bytes)
				: m_Bytes(std::move(bytes))
			{}

			size_t Read(void* buffer, size_t size, size_t count) override
			{
				if (size == 0 || count == 0)
					return 0;

				const size_t available = m_Bytes->size() - m_Position;
				const size_t items = (std::min)(count, available / size);
				std::memcpy(buffer, m_Bytes->data() + m_Position, items * size);
				m_Position += items * size;
				return items;
			}

			size_t Write(const void*, size_t, size_t) override { return 0; }

			aiReturn Seek(size_t offset, aiOrigin origin) override
			{
				size_t target = 0;
				switch (origin)
				{
				case aiOrigin_SET: target = offset; break;
				case aiOrigin_CUR: target = m_Position + offset; break;
				case aiOrigin_END: target = m_Bytes->size() - offset; break;
				default: return aiReturn_FAILURE;
				}
				if (target > m_Bytes->size())
					return aiReturn_FAILURE;
				m_Position = target;
				return aiReturn_SUCCESS;
			}

			size_t Tell() const override { return m_Position; }
			size_t FileSize() const override { return m_Bytes->size(); }
			void Flush() override {}

		private:
			std::shared_ptr<const std::vector<uint8_t>> m_Bytes;
			size_t m_Position = 0;
		};

		std::filesystem::path FromAssimpPath(const char* file)
		{
			const std::string utf8(file);
			return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
		}

		int HexValue(char c)
		{
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			return -1;
		}

		// glTF names its buffers and images by URI, so a space arrives as %20.
		std::string DecodePercent(const char* file)
		{
			std::string decoded;
			for (const char* c = file; *c; ++c)
			{
				if (*c == '%' && c[1] && c[2] && HexValue(c[1]) >= 0 && HexValue(c[2]) >= 0)
				{
					decoded.push_back(static_cast<char>(HexValue(c[1]) * 16 + HexValue(c[2])));
					c += 2;
				}
				else
				{
					decoded.push_back(*c);
				}
			}
			return decoded;
		}

	}

	std::string AssimpVfsIOSystem::ToAssimpPath(const std::filesystem::path& path)
	{
		const std::u8string utf8 = path.generic_u8string();
		return std::string(utf8.begin(), utf8.end());
	}

	std::filesystem::path AssimpVfsIOSystem::FindFile(const char* file) const
	{
		if (!file || !*file)
			return {};

		std::filesystem::path path = FromAssimpPath(file);
		if (VirtualFileSystem::Exists(path))
			return path;

		if (std::strchr(file, '%'))
		{
			path = FromAssimpPath(DecodePercent(file).c_str());
			if (VirtualFileSystem::Exists(path))
				return path;
		}
		return {};
	}

	bool AssimpVfsIOSystem::Exists(const char* file) const
	{
		return !FindFile(file).empty();
	}

	Assimp::IOStream* AssimpVfsIOSystem::Open(const char* file, const char* mode)
	{
		if (mode && (std::strchr(mode, 'w') || std::strchr(mode, 'a') || std::strchr(mode, '+')))
			return nullptr;

		const std::filesystem::path path = FindFile(file);
		if (path.empty())
			return nullptr;

		const std::string key = VirtualFileSystem::GetLogicalKey(path);
		if (m_CachedBytes && key == m_CachedKey)
			return new MemoryStream(m_CachedBytes);

		std::optional<std::vector<uint8_t>> bytes = VirtualFileSystem::ReadBytes(path);
		if (!bytes)
			return nullptr;

		m_CachedKey = key;
		m_CachedBytes = std::make_shared<const std::vector<uint8_t>>(std::move(*bytes));
		return new MemoryStream(m_CachedBytes);
	}

	void AssimpVfsIOSystem::Close(Assimp::IOStream* stream)
	{
		delete stream;
	}

	bool AssimpVfsIOSystem::ComparePaths(const char* one, const char* second) const
	{
		return VirtualFileSystem::GetLogicalKey(FromAssimpPath(one)) == VirtualFileSystem::GetLogicalKey(FromAssimpPath(second));
	}

}
