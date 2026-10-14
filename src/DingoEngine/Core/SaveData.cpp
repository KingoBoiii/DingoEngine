#include "depch.h"
#include "DingoEngine/Core/SaveData.h"
#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Core/Platform.h"
#include "DingoEngine/Core/PathText.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>

#if defined(DE_PLATFORM_LINUX)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace Dingo
{

	namespace
	{
		constexpr uint32_t k_MaxDepth = 256;
		constexpr size_t k_MaxSlotNameLength = 64;

		std::string DisplayPath(const std::filesystem::path& path)
		{
			const std::u8string text = path.u8string();
			return std::string(text.begin(), text.end());
		}

		std::filesystem::path WithSuffix(const std::filesystem::path& path, const char* suffix)
		{
			std::filesystem::path result = path;
			result += suffix;
			return result;
		}

		nlohmann::json ToJsonValue(const SaveData& value, bool& warnedNonFinite)
		{
			switch (value.GetType())
			{
				case SaveValueType::Null: return nullptr;
				case SaveValueType::Bool: return value.AsBool();
				case SaveValueType::Int: return value.AsInt();
				case SaveValueType::Float:
				{
					const double number = value.AsDouble();
					if (std::isfinite(number))
						return number;
					if (!warnedNonFinite)
					{
						DE_CORE_WARN("SaveData: a non-finite float is written as null.");
						warnedNonFinite = true;
					}
					return nullptr;
				}
				case SaveValueType::String: return value.AsString();
				case SaveValueType::Array:
				{
					nlohmann::json array = nlohmann::json::array();
					for (size_t i = 0; i < value.Size(); i++)
						array.push_back(ToJsonValue(value.At(i), warnedNonFinite));
					return array;
				}
				case SaveValueType::Object:
				{
					nlohmann::json object = nlohmann::json::object();
					for (size_t i = 0; i < value.Size(); i++)
						object[std::string(value.GetKey(i))] = ToJsonValue(value.At(i), warnedNonFinite);
					return object;
				}
			}
			return nullptr;
		}

		bool FromJsonValue(const nlohmann::json& json, SaveData& out, uint32_t depth, std::string& error)
		{
			if (depth > k_MaxDepth)
			{
				error = "nesting deeper than 256";
				return false;
			}

			switch (json.type())
			{
				case nlohmann::json::value_t::null: out = SaveData(); return true;
				case nlohmann::json::value_t::boolean: out = SaveData(json.get<bool>()); return true;
				case nlohmann::json::value_t::number_integer: out = SaveData(json.get<int64_t>()); return true;
				case nlohmann::json::value_t::number_unsigned:
				{
					out = SaveData(json.get<uint64_t>());
					return true;
				}
				case nlohmann::json::value_t::number_float: out = SaveData(json.get<double>()); return true;
				case nlohmann::json::value_t::string: out = SaveData(json.get_ref<const std::string&>()); return true;
				case nlohmann::json::value_t::array:
				{
					out = SaveData::MakeArray();
					for (const nlohmann::json& item : json)
					{
						SaveData& child = out.Push(SaveData());
						if (!FromJsonValue(item, child, depth + 1, error))
							return false;
					}
					return true;
				}
				case nlohmann::json::value_t::object:
				{
					out = SaveData::MakeObject();
					for (auto it = json.begin(); it != json.end(); ++it)
					{
						if (!FromJsonValue(it.value(), out[it.key()], depth + 1, error))
							return false;
					}
					return true;
				}
				default:
					error = "unsupported JSON value";
					return false;
			}
		}

		bool WriteDurably(const std::filesystem::path& path, std::string_view text, std::string& error)
		{
#if defined(DE_PLATFORM_WINDOWS)
			HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE)
			{
				error = std::format("cannot create it (error {})", GetLastError());
				return false;
			}
			size_t written = 0;
			while (written < text.size())
			{
				const DWORD chunk = static_cast<DWORD>((std::min)(text.size() - written, size_t(1) << 30));
				DWORD wrote = 0;
				if (!::WriteFile(file, text.data() + written, chunk, &wrote, nullptr) || wrote == 0)
				{
					error = std::format("write failed (error {})", GetLastError());
					CloseHandle(file);
					return false;
				}
				written += wrote;
			}
			if (!FlushFileBuffers(file))
			{
				error = std::format("flush failed (error {})", GetLastError());
				CloseHandle(file);
				return false;
			}
			CloseHandle(file);
			return true;
#else
			const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
			if (fd < 0)
			{
				error = std::format("cannot create it ({})", std::strerror(errno));
				return false;
			}
			size_t written = 0;
			while (written < text.size())
			{
				const ssize_t wrote = ::write(fd, text.data() + written, text.size() - written);
				if (wrote < 0 && errno == EINTR)
					continue;
				if (wrote <= 0)
				{
					error = std::format("write failed ({})", std::strerror(errno));
					::close(fd);
					return false;
				}
				written += static_cast<size_t>(wrote);
			}
			if (::fsync(fd) != 0)
			{
				error = std::format("fsync failed ({})", std::strerror(errno));
				::close(fd);
				return false;
			}
			::close(fd);
			return true;
#endif
		}

		bool MoveOver(const std::filesystem::path& from, const std::filesystem::path& to, std::string& error)
		{
#if defined(DE_PLATFORM_WINDOWS)
			// A virus scanner or indexer opening the fresh file makes the move fail for a moment.
			for (int attempt = 0; attempt < 5; attempt++)
			{
				if (MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
					return true;
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
			}
			error = std::format("cannot move '{}' to '{}' (error {})", DisplayPath(from), DisplayPath(to), GetLastError());
			return false;
#else
			std::error_code errorCode;
			std::filesystem::rename(from, to, errorCode);
			if (errorCode)
				error = std::format("cannot move '{}' to '{}' ({})", DisplayPath(from), DisplayPath(to), errorCode.message());
			return !errorCode;
#endif
		}

		void SyncDirectory([[maybe_unused]] const std::filesystem::path& directory)
		{
#if defined(DE_PLATFORM_LINUX)
			// The renames are only durable once the directory entry is.
			const int fd = ::open(directory.empty() ? "." : directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
			if (fd >= 0)
			{
				::fsync(fd);
				::close(fd);
			}
#endif
		}

		std::optional<SaveData> ReadOne(const std::filesystem::path& path, std::string& error)
		{
			std::ifstream stream(path, std::ios::binary);
			if (!stream)
			{
				error = "cannot open it";
				return std::nullopt;
			}
			std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
			if (stream.bad())
			{
				error = "read failed";
				return std::nullopt;
			}
			return SaveData::FromJson(text, &error);
		}

		// With <path> missing, a .tmp that parses is a write stopped between its renames.
		bool IsRecoverableTemporary(const std::filesystem::path& path)
		{
			std::error_code errorCode;
			if (std::filesystem::exists(path, errorCode))
				return false;
			const std::filesystem::path temporary = WithSuffix(path, ".tmp");
			std::string ignored;
			return std::filesystem::exists(temporary, errorCode) && ReadOne(temporary, ignored).has_value();
		}
	}

	SaveData::SaveData(const glm::vec2& value) : m_Type(SaveValueType::Array) { for (int i = 0; i < 2; i++) m_Items.push_back(std::make_unique<SaveData>(value[i])); }
	SaveData::SaveData(const glm::vec3& value) : m_Type(SaveValueType::Array) { for (int i = 0; i < 3; i++) m_Items.push_back(std::make_unique<SaveData>(value[i])); }
	SaveData::SaveData(const glm::vec4& value) : m_Type(SaveValueType::Array) { for (int i = 0; i < 4; i++) m_Items.push_back(std::make_unique<SaveData>(value[i])); }
	SaveData::SaveData(const glm::ivec2& value) : m_Type(SaveValueType::Array) { for (int i = 0; i < 2; i++) m_Items.push_back(std::make_unique<SaveData>(value[i])); }
	SaveData::SaveData(const glm::ivec3& value) : m_Type(SaveValueType::Array) { for (int i = 0; i < 3; i++) m_Items.push_back(std::make_unique<SaveData>(value[i])); }
	SaveData::SaveData(const glm::quat& value) : m_Type(SaveValueType::Array)
	{
		for (const float component : { value.x, value.y, value.z, value.w })
			m_Items.push_back(std::make_unique<SaveData>(component));
	}

	SaveData::SaveData(const SaveData& other)
		: m_Type(other.m_Type), m_Bool(other.m_Bool), m_Int(other.m_Int), m_Float(other.m_Float), m_String(other.m_String), m_Keys(other.m_Keys)
	{
		m_Items.reserve(other.m_Items.size());
		for (const std::unique_ptr<SaveData>& item : other.m_Items)
			m_Items.push_back(std::make_unique<SaveData>(*item));
	}

	SaveData::SaveData(SaveData&& other) noexcept
		: m_Type(std::exchange(other.m_Type, SaveValueType::Null)), m_Bool(other.m_Bool), m_Int(other.m_Int), m_Float(other.m_Float),
		m_String(std::move(other.m_String)), m_Items(std::move(other.m_Items)), m_Keys(std::move(other.m_Keys))
	{
		other.m_Items.clear();
		other.m_Keys.clear();
	}

	// Both build the new value before releasing the old tree, which may own other.
	SaveData& SaveData::operator=(const SaveData& other)
	{
		if (this != &other)
			*this = SaveData(other);
		return *this;
	}

	SaveData& SaveData::operator=(SaveData&& other) noexcept
	{
		if (this == &other)
			return *this;
		SaveData value(std::move(other));
		std::swap(m_Type, value.m_Type);
		std::swap(m_Bool, value.m_Bool);
		std::swap(m_Int, value.m_Int);
		std::swap(m_Float, value.m_Float);
		m_String.swap(value.m_String);
		m_Items.swap(value.m_Items);
		m_Keys.swap(value.m_Keys);
		return *this;
	}

	SaveData::~SaveData() = default;

	SaveData SaveData::MakeObject()
	{
		SaveData data;
		data.m_Type = SaveValueType::Object;
		return data;
	}

	SaveData SaveData::MakeArray()
	{
		SaveData data;
		data.m_Type = SaveValueType::Array;
		return data;
	}

	bool SaveData::AsBool(bool fallback) const
	{
		return m_Type == SaveValueType::Bool ? m_Bool : fallback;
	}

	int64_t SaveData::AsInt(int64_t fallback) const
	{
		if (m_Type == SaveValueType::Int)
			return m_Int;
		// 2^63 is exact in a double; anything from it up is out of range.
		if (m_Type == SaveValueType::Float && std::trunc(m_Float) == m_Float && m_Float >= -9223372036854775808.0 && m_Float < 9223372036854775808.0)
			return static_cast<int64_t>(m_Float);
		return fallback;
	}

	double SaveData::AsDouble(double fallback) const
	{
		if (m_Type == SaveValueType::Float)
			return m_Float;
		if (m_Type == SaveValueType::Int)
			return static_cast<double>(m_Int);
		return fallback;
	}

	std::string SaveData::AsString(std::string_view fallback) const
	{
		return m_Type == SaveValueType::String ? m_String : std::string(fallback);
	}

	bool SaveData::ReadComponents(float* out, size_t count) const
	{
		if (m_Type != SaveValueType::Array || m_Items.size() < count)
			return false;
		for (size_t i = 0; i < count; i++)
		{
			if (!m_Items[i]->IsNumber())
				return false;
			out[i] = m_Items[i]->AsFloat();
		}
		return true;
	}

	glm::vec2 SaveData::AsVec2(const glm::vec2& fallback) const
	{
		glm::vec2 value;
		return ReadComponents(&value.x, 2) ? value : fallback;
	}

	glm::vec3 SaveData::AsVec3(const glm::vec3& fallback) const
	{
		glm::vec3 value;
		return ReadComponents(&value.x, 3) ? value : fallback;
	}

	glm::vec4 SaveData::AsVec4(const glm::vec4& fallback) const
	{
		glm::vec4 value;
		return ReadComponents(&value.x, 4) ? value : fallback;
	}

	glm::ivec2 SaveData::AsIVec2(const glm::ivec2& fallback) const
	{
		if (m_Type != SaveValueType::Array || m_Items.size() < 2 || !m_Items[0]->IsInt() || !m_Items[1]->IsInt())
			return fallback;
		return glm::ivec2(static_cast<int>(m_Items[0]->AsInt()), static_cast<int>(m_Items[1]->AsInt()));
	}

	glm::ivec3 SaveData::AsIVec3(const glm::ivec3& fallback) const
	{
		if (m_Type != SaveValueType::Array || m_Items.size() < 3 || !m_Items[0]->IsInt() || !m_Items[1]->IsInt() || !m_Items[2]->IsInt())
			return fallback;
		return glm::ivec3(static_cast<int>(m_Items[0]->AsInt()), static_cast<int>(m_Items[1]->AsInt()), static_cast<int>(m_Items[2]->AsInt()));
	}

	glm::quat SaveData::AsQuat(const glm::quat& fallback) const
	{
		float components[4];
		if (!ReadComponents(components, 4))
			return fallback;
		const glm::quat value(components[3], components[0], components[1], components[2]);
		const float length = glm::length(value);
		if (!(length > 0.0f) || !std::isfinite(length))
			return fallback;
		return value / length;
	}

	size_t SaveData::LowerBound(std::string_view key) const
	{
		return static_cast<size_t>(std::lower_bound(m_Keys.begin(), m_Keys.end(), key, [](const std::string& a, std::string_view b) { return std::string_view(a) < b; }) - m_Keys.begin());
	}

	SaveData& SaveData::operator[](std::string_view key)
	{
		if (m_Type != SaveValueType::Object)
		{
			if (m_Type != SaveValueType::Null)
				DE_CORE_ERROR("SaveData: operator[](\"{}\") on a value that is not an object; it becomes one.", key);
			*this = MakeObject();
		}

		const size_t index = LowerBound(key);
		if (index < m_Keys.size() && m_Keys[index] == key)
			return *m_Items[index];

		auto value = std::make_unique<SaveData>();
		SaveData& member = *value;
		// Room first, so the insert below can't throw after the key went in; doubled, or each insert copies.
		if (m_Items.size() == m_Items.capacity())
			m_Items.reserve((std::max)(size_t(4), m_Items.capacity() * 2));
		m_Keys.insert(m_Keys.begin() + index, std::string(key));
		m_Items.insert(m_Items.begin() + index, std::move(value));
		return member;
	}

	const SaveData& SaveData::operator[](std::string_view key) const
	{
		static const SaveData s_Null;
		const SaveData* value = Find(key);
		return value ? *value : s_Null;
	}

	SaveData* SaveData::Find(std::string_view key)
	{
		return const_cast<SaveData*>(static_cast<const SaveData*>(this)->Find(key));
	}

	const SaveData* SaveData::Find(std::string_view key) const
	{
		if (m_Type != SaveValueType::Object)
			return nullptr;
		const size_t index = LowerBound(key);
		if (index < m_Keys.size() && m_Keys[index] == key)
			return m_Items[index].get();
		return nullptr;
	}

	bool SaveData::Remove(std::string_view key)
	{
		if (m_Type != SaveValueType::Object)
			return false;
		const size_t index = LowerBound(key);
		if (index >= m_Keys.size() || m_Keys[index] != key)
			return false;
		m_Keys.erase(m_Keys.begin() + index);
		m_Items.erase(m_Items.begin() + index);
		return true;
	}

	std::string_view SaveData::GetKey(size_t index) const
	{
		return index < m_Keys.size() ? std::string_view(m_Keys[index]) : std::string_view();
	}

	SaveData& SaveData::Push(SaveData value)
	{
		if (m_Type != SaveValueType::Array)
		{
			if (m_Type != SaveValueType::Null)
				DE_CORE_ERROR("SaveData: Push on a value that is not an array; it becomes one.");
			*this = MakeArray();
		}
		m_Items.push_back(std::make_unique<SaveData>(std::move(value)));
		return *m_Items.back();
	}

	SaveData& SaveData::At(size_t index)
	{
		if (index < m_Items.size())
			return *m_Items[index];
		DE_CORE_ERROR("SaveData: At({}) past the end ({} items); the value goes nowhere.", index, m_Items.size());
		thread_local SaveData t_Scratch;
		t_Scratch = SaveData();
		return t_Scratch;
	}

	const SaveData& SaveData::At(size_t index) const
	{
		static const SaveData s_Null;
		return index < m_Items.size() ? *m_Items[index] : s_Null;
	}

	void SaveData::Clear()
	{
		m_Items.clear();
		m_Keys.clear();
	}

	bool SaveData::operator==(const SaveData& other) const
	{
		if (m_Type != other.m_Type)
			return false;
		switch (m_Type)
		{
			case SaveValueType::Null: return true;
			case SaveValueType::Bool: return m_Bool == other.m_Bool;
			case SaveValueType::Int: return m_Int == other.m_Int;
			case SaveValueType::Float: return m_Float == other.m_Float;
			case SaveValueType::String: return m_String == other.m_String;
			case SaveValueType::Array:
			case SaveValueType::Object:
				return m_Keys == other.m_Keys && std::equal(m_Items.begin(), m_Items.end(), other.m_Items.begin(), other.m_Items.end(),
					[](const std::unique_ptr<SaveData>& a, const std::unique_ptr<SaveData>& b) { return *a == *b; });
		}
		return false;
	}

	std::string SaveData::ToJson(bool pretty) const
	{
		bool warnedNonFinite = false;
		const nlohmann::json json = ToJsonValue(*this, warnedNonFinite);
		return json.dump(pretty ? 2 : -1, ' ', false, nlohmann::json::error_handler_t::replace);
	}

	std::optional<SaveData> SaveData::FromJson(std::string_view json, std::string* error)
	{
		std::string reason;
		try
		{
			const nlohmann::json parsed = nlohmann::json::parse(json.begin(), json.end());
			SaveData data;
			if (FromJsonValue(parsed, data, 0, reason))
				return data;
		}
		catch (const nlohmann::json::exception& exception)
		{
			reason = exception.what();
		}
		if (error)
			*error = std::move(reason);
		return std::nullopt;
	}

	bool SaveData::WriteFile(const std::filesystem::path& path) const
	{
		std::string text = ToJson(true);
		text.push_back('\n');

		// Every write shares <path>.tmp, so two at once would interleave in it.
		static std::mutex s_WriteMutex;
		std::lock_guard lock(s_WriteMutex);

		std::string error;
		const std::filesystem::path directory = path.parent_path();
		if (!directory.empty())
		{
			std::error_code errorCode;
			std::filesystem::create_directories(directory, errorCode);
			if (errorCode)
			{
				DE_CORE_ERROR("SaveData: cannot create '{}': {}", DisplayPath(directory), errorCode.message());
				return false;
			}
		}

		const std::filesystem::path temporary = WithSuffix(path, ".tmp");
		// The recovered .tmp is the generation ReadFile returns; promoted, it rotates to .bak below
		// instead of being overwritten.
		if (IsRecoverableTemporary(path) && !MoveOver(temporary, path, error))
			DE_CORE_WARN("SaveData: cannot recover '{}': {}", DisplayPath(temporary), error);

		std::error_code errorCode;

		if (!WriteDurably(temporary, text, error))
		{
			DE_CORE_ERROR("SaveData: cannot write '{}': {}", DisplayPath(temporary), error);
			std::error_code ignored;
			std::filesystem::remove(temporary, ignored);
			return false;
		}

		if (std::filesystem::exists(path, errorCode) && !MoveOver(path, WithSuffix(path, ".bak"), error))
		{
			DE_CORE_ERROR("SaveData: {}", error);
			std::error_code ignored;
			std::filesystem::remove(temporary, ignored);
			return false;
		}

		if (!MoveOver(temporary, path, error))
		{
			DE_CORE_ERROR("SaveData: {}", error);
			return false;
		}

		SyncDirectory(directory);
		return true;
	}

	std::optional<SaveData> SaveData::ReadFile(const std::filesystem::path& path, std::string* error, std::filesystem::path* source)
	{
		std::error_code errorCode;
		const bool mainExists = std::filesystem::exists(path, errorCode);

		std::string mainError = "not found";
		if (mainExists)
		{
			if (std::optional<SaveData> data = ReadOne(path, mainError))
			{
				if (source)
					*source = path;
				return data;
			}
		}

		// With <path> gone, a write stopped between its renames: its .tmp is complete if it parses.
		std::vector<std::filesystem::path> fallbacks;
		if (!mainExists)
			fallbacks.push_back(WithSuffix(path, ".tmp"));
		fallbacks.push_back(WithSuffix(path, ".bak"));

		std::string fallbackErrors;
		bool anyExists = mainExists;
		for (const std::filesystem::path& fallback : fallbacks)
		{
			if (!std::filesystem::exists(fallback, errorCode))
				continue;
			anyExists = true;
			std::string fallbackError;
			if (std::optional<SaveData> data = ReadOne(fallback, fallbackError))
			{
				DE_CORE_WARN("SaveData: '{}' {}; read '{}'.", DisplayPath(path), mainExists ? "is unreadable (" + mainError + ")" : std::string("is missing"), DisplayPath(fallback));
				if (source)
					*source = fallback;
				return data;
			}
			fallbackErrors += std::format("; '{}': {}", DisplayPath(fallback), fallbackError);
		}

		const std::string reason = std::format("'{}': {}{}", DisplayPath(path), mainError, fallbackErrors);
		if (anyExists)
			DE_CORE_ERROR("SaveData: cannot read a save: {}", reason);
		if (error)
			*error = reason;
		return std::nullopt;
	}

	std::filesystem::path SaveSlots::GetDirectory()
	{
		if (Application::HasInstance())
			return Application::Get().GetSaveDirectory();
		return Platform::GetUserDataDir(Internal::PathToUtf8(Platform::GetExecutablePath().stem())) / "saves";
	}

	bool SaveSlots::IsValidName(std::string_view slot)
	{
		if (slot.empty() || slot.size() > k_MaxSlotNameLength)
			return false;
		return std::all_of(slot.begin(), slot.end(), [](char c)
		{
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
		});
	}

	std::filesystem::path SaveSlots::GetPath(std::string_view slot)
	{
		return GetDirectory() / (std::string(slot) + ".json");
	}

	bool SaveSlots::Write(std::string_view slot, const SaveData& data)
	{
		if (!IsValidName(slot))
		{
			DE_CORE_ERROR("SaveSlots: '{}' is not a valid slot name (1 to 64 of A-Z, a-z, 0-9, '_', '-').", slot);
			return false;
		}
		return data.WriteFile(GetPath(slot));
	}

	std::optional<SaveData> SaveSlots::Read(std::string_view slot, std::string* error)
	{
		if (!IsValidName(slot))
		{
			DE_CORE_ERROR("SaveSlots: '{}' is not a valid slot name (1 to 64 of A-Z, a-z, 0-9, '_', '-').", slot);
			if (error)
				*error = "invalid slot name";
			return std::nullopt;
		}
		return SaveData::ReadFile(GetPath(slot), error);
	}

	bool SaveSlots::Exists(std::string_view slot)
	{
		if (!IsValidName(slot))
			return false;
		std::error_code errorCode;
		const std::filesystem::path path = GetPath(slot);
		return std::filesystem::exists(path, errorCode) || std::filesystem::exists(WithSuffix(path, ".bak"), errorCode) || IsRecoverableTemporary(path);
	}

	bool SaveSlots::Delete(std::string_view slot)
	{
		if (!IsValidName(slot))
			return false;
		const std::filesystem::path path = GetPath(slot);
		bool removedAll = true;
		for (const std::filesystem::path& file : { path, WithSuffix(path, ".bak"), WithSuffix(path, ".tmp") })
		{
			std::error_code errorCode;
			std::filesystem::remove(file, errorCode);
			if (errorCode)
			{
				DE_CORE_ERROR("SaveSlots: cannot delete '{}': {}", DisplayPath(file), errorCode.message());
				removedAll = false;
			}
		}
		return removedAll;
	}

	std::vector<SaveSlotInfo> SaveSlots::List()
	{
		std::vector<SaveSlotInfo> slots;
		std::error_code errorCode;
		std::filesystem::directory_iterator iterator(GetDirectory(), errorCode);
		if (errorCode)
			return slots;

		for (; iterator != std::filesystem::directory_iterator(); iterator.increment(errorCode))
		{
			if (errorCode)
				break;
			const std::filesystem::directory_entry& entry = *iterator;
			std::error_code entryError;
			if (!entry.is_regular_file(entryError))
				continue;

			// u8string: string() throws on Windows for a name outside the ANSI code page.
			const std::u8string fileName = entry.path().filename().u8string();
			std::u8string_view name = fileName;
			bool isBackup = false;
			if (name.ends_with(u8".json"))
				name.remove_suffix(5);
			else if (name.ends_with(u8".json.bak"))
			{
				name.remove_suffix(9);
				isBackup = true;
			}
			else if (name.ends_with(u8".json.tmp"))
				name.remove_suffix(9);
			else
				continue;
			const std::string slot(name.begin(), name.end());
			if (!IsValidName(slot))
				continue;
			if (fileName.ends_with(u8".tmp") && !IsRecoverableTemporary(GetPath(slot)))
				continue;

			const std::filesystem::file_time_type time = entry.last_write_time(entryError);
			auto existing = std::find_if(slots.begin(), slots.end(), [&](const SaveSlotInfo& info) { return info.Name == slot; });
			if (existing == slots.end())
				slots.push_back({ slot, time });
			else if (!isBackup)
				existing->LastWriteTime = time;
		}

		std::sort(slots.begin(), slots.end(), [](const SaveSlotInfo& a, const SaveSlotInfo& b)
		{
			return a.LastWriteTime != b.LastWriteTime ? a.LastWriteTime > b.LastWriteTime : a.Name < b.Name;
		});
		return slots;
	}

}
