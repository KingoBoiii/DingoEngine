#pragma once

const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
};

#ifdef DE_DEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif
