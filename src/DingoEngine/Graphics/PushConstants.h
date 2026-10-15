#pragma once
#include "DingoEngine/Graphics/Shader.h"

#include <string_view>

namespace Dingo::Internal
{

	bool CopyPushConstants(const Shader* shader, const void* data, uint32_t size, uint8_t* destination, std::string_view ownerKind, std::string_view ownerName);

}
