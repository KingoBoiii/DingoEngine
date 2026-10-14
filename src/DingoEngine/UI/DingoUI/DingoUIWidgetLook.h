#pragma once
#include <glm/glm.hpp>

namespace Dingo::Internal::UILook
{

	// The one place the widgets' hard-coded look lives; the style system replaces it. Sizes are
	// design pixels.
	inline constexpr float k_TextSize = 18.0f;
	inline constexpr float k_CornerRadius = 6.0f;
	inline constexpr float k_ButtonPaddingX = 14.0f;
	inline constexpr float k_ButtonPaddingY = 7.0f;
	inline constexpr float k_RowPaddingX = 10.0f;
	inline constexpr float k_RowPaddingY = 5.0f;
	inline constexpr float k_Gap = 8.0f;
	inline constexpr float k_ToggleWidth = 40.0f;
	inline constexpr float k_ToggleHeight = 22.0f;
	inline constexpr float k_ToggleKnob = 16.0f;
	inline constexpr float k_SliderHeight = 24.0f;
	inline constexpr float k_SliderTrackHeight = 6.0f;
	inline constexpr float k_SliderHandle = 16.0f;
	inline constexpr float k_ProgressWidth = 200.0f;
	inline constexpr float k_ProgressHeight = 14.0f;
	inline constexpr float k_ScrollThumbWidth = 4.0f;
	inline constexpr float k_ScrollThumbMargin = 2.0f;
	inline constexpr float k_ScrollThumbMin = 16.0f;

	inline const glm::vec4 k_Text{ 0.95f, 0.96f, 1.0f, 1.0f };
	inline const glm::vec4 k_TextDisabled{ 0.55f, 0.57f, 0.63f, 1.0f };
	inline const glm::vec4 k_Idle{ 0.22f, 0.27f, 0.42f, 1.0f };
	inline const glm::vec4 k_Hover{ 0.31f, 0.38f, 0.58f, 1.0f };
	inline const glm::vec4 k_Pressed{ 0.16f, 0.2f, 0.33f, 1.0f };
	inline const glm::vec4 k_Disabled{ 0.17f, 0.18f, 0.23f, 1.0f };
	inline const glm::vec4 k_Accent{ 0.3f, 0.55f, 0.95f, 1.0f };
	inline const glm::vec4 k_AccentHover{ 0.4f, 0.64f, 1.0f, 1.0f };
	inline const glm::vec4 k_Track{ 0.14f, 0.16f, 0.24f, 1.0f };
	inline const glm::vec4 k_Knob{ 0.96f, 0.97f, 1.0f, 1.0f };
	inline const glm::vec4 k_Transparent{ 0.0f };
	inline const glm::vec4 k_ScrollThumb{ 0.8f, 0.85f, 1.0f, 0.45f };

}
