#pragma once
#include "DingoEngine/Common.h"

#include <glm/glm.hpp>

#include <charconv>
#include <optional>
#include <string_view>

namespace Dingo::Internal
{

	constexpr glm::ivec2 k_MinLaunchWindowSize = { 640, 360 };
	constexpr glm::ivec2 k_MaxLaunchWindowSize = { 16384, 16384 };

	inline bool IsLaunchFlagOn(std::optional<std::string_view> value)
	{
		return value && *value != "off" && *value != "false" && *value != "0" && *value != "no";
	}

	inline std::optional<GraphicsAPI> ParseGraphicsFlag(std::string_view value)
	{
		if (value == "vulkan")
			return GraphicsAPI::Vulkan;
		if (value == "dx11" || value == "directx11")
			return GraphicsAPI::DirectX11;
		if (value == "dx12" || value == "directx12")
			return GraphicsAPI::DirectX12;
		if (value == "headless")
			return GraphicsAPI::Headless;
		return std::nullopt;
	}

	inline std::optional<glm::ivec2> ParseWindowFlag(std::string_view value)
	{
		glm::ivec2 size{};
		const char* end = value.data() + value.size();
		const auto [afterWidth, widthError] = std::from_chars(value.data(), end, size.x);
		if (widthError != std::errc() || afterWidth == end || (*afterWidth != 'x' && *afterWidth != 'X'))
			return std::nullopt;
		const auto [afterHeight, heightError] = std::from_chars(afterWidth + 1, end, size.y);
		if (heightError != std::errc() || afterHeight != end)
			return std::nullopt;
		if (size.x < k_MinLaunchWindowSize.x || size.y < k_MinLaunchWindowSize.y || size.x > k_MaxLaunchWindowSize.x || size.y > k_MaxLaunchWindowSize.y)
			return std::nullopt;
		return size;
	}

}
