#include "depch.h"
#include "DingoEngine/Graphics/ShaderIncludes.h"
#include "DingoEngine/Graphics/EngineShaders.h"
#include "DingoEngine/Asset/AssetPath.h"
#include "DingoEngine/Core/FileSystem.h"

#include <set>

namespace Dingo::Internal
{

	namespace
	{
		constexpr uint32_t k_MaxIncludeDepth = 32;
		constexpr std::string_view k_EnginePrefix = "DingoEngine/";

		// Where a source came from, which decides where its quoted includes are looked up.
		struct Origin
		{
			std::filesystem::path Directory;
			bool Engine = false;
			bool Inline = false;
		};

		struct Include
		{
			std::string Name;
			bool Angled = false;
		};

		// "#include", optional spaces, then "name" or <name>; anything else on the line is ignored.
		std::optional<Include> ParseInclude(std::string_view line)
		{
			size_t pos = line.find_first_not_of(" \t");
			if (pos == std::string_view::npos || line[pos] != '#')
				return std::nullopt;
			pos = line.find_first_not_of(" \t", pos + 1);
			if (pos == std::string_view::npos || line.compare(pos, 7, "include") != 0)
				return std::nullopt;
			pos = line.find_first_not_of(" \t", pos + 7);
			if (pos == std::string_view::npos || (line[pos] != '"' && line[pos] != '<'))
				return std::nullopt;

			const bool angled = line[pos] == '<';
			const size_t end = line.find(angled ? '>' : '"', pos + 1);
			if (end == std::string_view::npos || end == pos + 1)
				return std::nullopt;
			return Include{ std::string(line.substr(pos + 1, end - pos - 1)), angled };
		}

		class Expander
		{
		public:
			Expander(const std::string& shaderName, std::vector<std::filesystem::path>& includedFiles)
				: m_ShaderName(shaderName), m_IncludedFiles(includedFiles)
			{}

			bool Expand(std::string_view source, const Origin& origin, uint32_t depth, std::string& out)
			{
				size_t lineStart = 0;
				while (lineStart <= source.size())
				{
					const size_t newline = source.find('\n', lineStart);
					const size_t lineEnd = newline == std::string_view::npos ? source.size() : newline;
					const std::string_view line = source.substr(lineStart, lineEnd - lineStart);

					if (const std::optional<Include> include = ParseInclude(line))
					{
						if (!Paste(*include, origin, depth, out))
							return false;
					}
					else
					{
						out.append(line);
					}

					if (newline == std::string_view::npos)
						break;
					out.push_back('\n');
					lineStart = newline + 1;
				}
				return true;
			}

		private:
			bool Paste(const Include& include, const Origin& origin, uint32_t depth, std::string& out)
			{
				if (depth >= k_MaxIncludeDepth)
				{
					DE_CORE_ERROR("Shader '{}': #include \"{}\" nests more than {} levels deep.", m_ShaderName, include.Name, k_MaxIncludeDepth);
					return false;
				}

				// An engine file: <DingoEngine/Name.glsl>, or a quoted name inside an engine file.
				std::string engineName;
				if (include.Angled)
				{
					if (!include.Name.starts_with(k_EnginePrefix))
					{
						DE_CORE_ERROR("Shader '{}': #include <{}> isn't an engine shader; only <DingoEngine/...> takes angle brackets.", m_ShaderName, include.Name);
						return false;
					}
					engineName = include.Name.substr(k_EnginePrefix.size());
				}
				else if (origin.Engine)
				{
					engineName = include.Name;
				}

				if (!engineName.empty())
				{
					if (!m_Pasted.insert("<engine>/" + engineName).second)
						return true;

					const std::filesystem::path file = FindEngineShaderFile(engineName);
					if (!file.empty())
						return PasteFile(file, Origin{ {}, true, false }, include.Name, depth, out);

					const std::string_view embedded = FindEmbeddedEngineShader(engineName);
					if (embedded.empty())
					{
						DE_CORE_ERROR("Shader '{}': #include \"{}\": there is no engine shader '{}'.", m_ShaderName, include.Name, engineName);
						return false;
					}
					return Expand(embedded, Origin{ {}, true, false }, depth + 1, out);
				}

				std::filesystem::path file = origin.Directory / std::filesystem::path(include.Name);
				if (origin.Inline)
					file = ResolveRawAssetPath(include.Name);

				std::error_code ec;
				const std::filesystem::path canonical = std::filesystem::weakly_canonical(file, ec);
				if (!m_Pasted.insert((ec ? file : canonical).generic_string()).second)
					return true;

				return PasteFile(file, Origin{ file.parent_path(), false, false }, include.Name, depth, out);
			}

			bool PasteFile(const std::filesystem::path& file, const Origin& origin, const std::string& requested, uint32_t depth, std::string& out)
			{
				// Watched before it is looked for or read, so creating a missing file, or fixing one that
				// fails to read, reloads the shader (a missing file reads as the epoch).
				if (std::ranges::find(m_IncludedFiles, file) == m_IncludedFiles.end())
					m_IncludedFiles.push_back(file);

				std::error_code ec;
				if (!std::filesystem::exists(file, ec))
				{
					DE_CORE_ERROR("Shader '{}': #include \"{}\" not found ({}).", m_ShaderName, requested, file.generic_string());
					return false;
				}

				const std::string text = FileSystem::ReadTextFile(file);
				return Expand(text, origin, depth + 1, out);
			}

			const std::string& m_ShaderName;
			std::vector<std::filesystem::path>& m_IncludedFiles;
			std::set<std::string> m_Pasted;
		};
	}

	std::optional<std::string> ExpandShaderIncludes(const std::string& source, const std::filesystem::path& sourcePath, const std::string& shaderName, std::vector<std::filesystem::path>& includedFiles, bool engineSource)
	{
		Origin origin;
		origin.Engine = engineSource && sourcePath.empty();
		origin.Inline = sourcePath.empty() && !origin.Engine;
		origin.Directory = sourcePath.parent_path();

		std::string expanded;
		expanded.reserve(source.size());
		Expander expander(shaderName, includedFiles);
		if (!expander.Expand(source, origin, 0, expanded))
			return std::nullopt;
		return expanded;
	}

}
