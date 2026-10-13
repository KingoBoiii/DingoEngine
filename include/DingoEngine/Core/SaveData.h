#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace Dingo
{

	enum class SaveValueType : uint8_t
	{
		Null,
		Bool,
		Int,
		Float,
		String,
		Array,
		Object
	};

	// A typed key/value tree for save games and settings, stored as JSON. Vectors and quaternions
	// are written as arrays of numbers. An object keeps its keys sorted, so a file always comes out
	// the same and GetKey(i) walks them in order.
	class SaveData
	{
	public:
		SaveData() = default;
		SaveData(bool value) : m_Type(SaveValueType::Bool), m_Bool(value) {}
		template<std::integral T> requires (!std::same_as<T, bool>)
		SaveData(T value) : m_Type(SaveValueType::Int), m_Int(static_cast<int64_t>(value)) {}
		template<typename T> requires std::is_enum_v<T>
		SaveData(T value) : m_Type(SaveValueType::Int), m_Int(static_cast<int64_t>(value)) {}
		template<std::floating_point T>
		SaveData(T value) : m_Type(SaveValueType::Float), m_Float(static_cast<double>(value)) {}
		SaveData(std::nullptr_t) {}
		SaveData(const char* value) : m_Type(SaveValueType::String), m_String(value ? value : "") {}
		SaveData(const char8_t* value) : SaveData(std::u8string_view(value ? value : u8"")) {}
		SaveData(std::u8string_view value) : m_Type(SaveValueType::String), m_String(value.begin(), value.end()) {}
		// Any other pointer would otherwise convert to bool.
		template<typename T> requires (!std::same_as<std::remove_cv_t<T>, char> && !std::same_as<std::remove_cv_t<T>, char8_t>)
		SaveData(T*) = delete;
		SaveData(std::string_view value) : m_Type(SaveValueType::String), m_String(value) {}
		SaveData(std::string value) : m_Type(SaveValueType::String), m_String(std::move(value)) {}
		SaveData(const glm::vec2& value);
		SaveData(const glm::vec3& value);
		SaveData(const glm::vec4& value);
		SaveData(const glm::ivec2& value);
		SaveData(const glm::ivec3& value);
		SaveData(const glm::quat& value);

		static SaveData MakeObject();
		static SaveData MakeArray();

		SaveValueType GetType() const { return m_Type; }
		bool IsNull() const { return m_Type == SaveValueType::Null; }
		bool IsBool() const { return m_Type == SaveValueType::Bool; }
		bool IsInt() const { return m_Type == SaveValueType::Int; }
		bool IsFloat() const { return m_Type == SaveValueType::Float; }
		bool IsNumber() const { return IsInt() || IsFloat(); }
		bool IsString() const { return m_Type == SaveValueType::String; }
		bool IsArray() const { return m_Type == SaveValueType::Array; }
		bool IsObject() const { return m_Type == SaveValueType::Object; }

		// The fallback when the value has another type. A number reads as either kind: a Float
		// reads as an Int only when it is whole and in range.
		bool AsBool(bool fallback = false) const;
		int64_t AsInt(int64_t fallback = 0) const;
		double AsDouble(double fallback = 0.0) const;
		float AsFloat(float fallback = 0.0f) const { return static_cast<float>(AsDouble(fallback)); }
		std::string AsString(std::string_view fallback = {}) const;
		// An array of at least as many numbers as the vector has components (extra ones ignored).
		glm::vec2 AsVec2(const glm::vec2& fallback = glm::vec2(0.0f)) const;
		glm::vec3 AsVec3(const glm::vec3& fallback = glm::vec3(0.0f)) const;
		glm::vec4 AsVec4(const glm::vec4& fallback = glm::vec4(0.0f)) const;
		glm::ivec2 AsIVec2(const glm::ivec2& fallback = glm::ivec2(0)) const;
		glm::ivec3 AsIVec3(const glm::ivec3& fallback = glm::ivec3(0)) const;
		// Stored x, y, z, w; normalized on read.
		glm::quat AsQuat(const glm::quat& fallback = glm::quat(1.0f, 0.0f, 0.0f, 0.0f)) const;

		// The member's value read as T, or the fallback when the member is missing or of another
		// type, or an integer out of T's range. An enum reads its stored integer.
		template<typename T>
		T Get(std::string_view key, const T& fallback) const;
		std::string Get(std::string_view key, const char* fallback) const { return Get(key, std::string(fallback ? fallback : "")); }

		// Objects. The non-const operator[] adds a missing member (as Null) and turns a Null value
		// into an object; any other value is replaced by an empty object, with an error logged. The
		// const one returns a Null for a missing key.
		SaveData& operator[](std::string_view key);
		const SaveData& operator[](std::string_view key) const;
		SaveData* Find(std::string_view key);
		const SaveData* Find(std::string_view key) const;
		bool Contains(std::string_view key) const { return Find(key) != nullptr; }
		bool Remove(std::string_view key);
		// The i-th key in sorted order; empty for an array or past the end.
		std::string_view GetKey(size_t index) const;

		// Arrays. Push turns a Null value into an array; any other non-array is replaced by an empty
		// one, with an error logged. At and operator[](index) also reach an object's values in key
		// order; past the end they return a Null (the non-const one a scratch value, with an error).
		SaveData& Push(SaveData value);
		SaveData& At(size_t index);
		const SaveData& At(size_t index) const;
		SaveData& operator[](size_t index) { return At(index); }
		const SaveData& operator[](size_t index) const { return At(index); }

		// Members of an object or items of an array; 0 otherwise.
		size_t Size() const { return m_Items.size(); }
		bool IsEmpty() const { return m_Items.empty(); }
		void Clear();

		bool operator==(const SaveData& other) const;

		// Non-finite floats are written as null (with a warning): JSON has no NaN or infinity.
		// Strings are written as UTF-8: invalid bytes come back as U+FFFD.
		std::string ToJson(bool pretty = true) const;
		// nullopt on a syntax error or nesting deeper than 256, with the reason in error.
		static std::optional<SaveData> FromJson(std::string_view json, std::string* error = nullptr);

		// Atomic: the JSON goes to <path>.tmp, flushed to disk, then the current file becomes
		// <path>.bak and the new one takes its place, so a crash or kill at any point leaves a
		// complete file under one of the three names. Writes from several threads take turns.
		// Creates the directory. False (logged) on failure.
		bool WriteFile(const std::filesystem::path& path) const;
		// Reads <path>; when that is missing, a complete <path>.tmp (a write stopped between its two
		// renames), then <path>.bak; when unreadable, <path>.bak (each fallback logged). nullopt when
		// none parses; source receives the file actually read.
		static std::optional<SaveData> ReadFile(const std::filesystem::path& path, std::string* error = nullptr, std::filesystem::path* source = nullptr);

	private:
		size_t LowerBound(std::string_view key) const;
		bool ReadComponents(float* out, size_t count) const;

	private:
		SaveValueType m_Type = SaveValueType::Null;
		bool m_Bool = false;
		int64_t m_Int = 0;
		double m_Float = 0.0;
		std::string m_String;
		std::vector<SaveData> m_Items;		// array items, or object values parallel to m_Keys
		std::vector<std::string> m_Keys;	// sorted
	};

	template<typename T>
	T SaveData::Get(std::string_view key, const T& fallback) const
	{
		const SaveData* value = Find(key);
		if (!value)
			return fallback;

		if constexpr (std::same_as<T, bool>)
			return value->AsBool(fallback);
		else if constexpr (std::is_enum_v<T>)
			return value->IsInt() ? static_cast<T>(value->AsInt()) : fallback;
		else if constexpr (std::same_as<std::remove_cv_t<T>, char> || std::same_as<std::remove_cv_t<T>, char8_t> || std::same_as<std::remove_cv_t<T>, char16_t> || std::same_as<std::remove_cv_t<T>, char32_t> || std::same_as<std::remove_cv_t<T>, wchar_t>)
			static_assert(sizeof(T) == 0, "SaveData::Get: read a character as an integer type or a std::string");
		else if constexpr (std::integral<T>)
		{
			if (!value->IsNumber())
				return fallback;
			const int64_t number = value->AsInt(static_cast<int64_t>(fallback));
			if (!std::in_range<T>(number))
				return fallback;
			return static_cast<T>(number);
		}
		else if constexpr (std::floating_point<T>)
			return static_cast<T>(value->AsDouble(static_cast<double>(fallback)));
		else if constexpr (std::same_as<T, std::string>)
			return value->AsString(fallback);
		else if constexpr (std::same_as<T, glm::vec2>)
			return value->AsVec2(fallback);
		else if constexpr (std::same_as<T, glm::vec3>)
			return value->AsVec3(fallback);
		else if constexpr (std::same_as<T, glm::vec4>)
			return value->AsVec4(fallback);
		else if constexpr (std::same_as<T, glm::ivec2>)
			return value->AsIVec2(fallback);
		else if constexpr (std::same_as<T, glm::ivec3>)
			return value->AsIVec3(fallback);
		else if constexpr (std::same_as<T, glm::quat>)
			return value->AsQuat(fallback);
		else if constexpr (std::same_as<T, SaveData>)
			return *value;
		else
			static_assert(sizeof(T) == 0, "SaveData::Get: unsupported type");
	}

	struct SaveSlotInfo
	{
		std::string Name;
		std::filesystem::file_time_type LastWriteTime;
	};

	// Named save files under Application::GetSaveDirectory(): <slot>.json beside its .bak.
	// A slot name is 1 to 64 of A-Z, a-z, 0-9, '_' and '-'; any other name fails (logged).
	class SaveSlots
	{
	public:
		// Application::GetSaveDirectory(), or GetUserDataDir(<executable name>)/saves outside an application.
		static std::filesystem::path GetDirectory();
		static std::filesystem::path GetPath(std::string_view slot);
		static bool IsValidName(std::string_view slot);

		static bool Write(std::string_view slot, const SaveData& data);
		static std::optional<SaveData> Read(std::string_view slot, std::string* error = nullptr);
		// True when the slot or its .bak is on disk.
		static bool Exists(std::string_view slot);
		// Removes the slot, its .bak and any leftover .tmp. True when nothing of it is left.
		static bool Delete(std::string_view slot);
		// Every slot in the directory (a .bak alone counts), most recently written first.
		static std::vector<SaveSlotInfo> List();
	};

}
