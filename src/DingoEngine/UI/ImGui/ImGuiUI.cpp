#include "depch.h"

#include "DingoEngine/UI/UI.h"

#include <imgui.h>

#include <cstdarg>

// ImGui-backed implementation of the engine's public UI facade (DingoEngine/UI/UI.h).
// This is the only place that includes <imgui.h>; clients see only Dingo::UI.

namespace Dingo::UI
{

	namespace
	{
		ImVec4 ToImVec4(const Color& c)
		{
			return ImVec4(c.R, c.G, c.B, c.A);
		}
	}

	// --- Windows ---------------------------------------------------------

	bool Begin(const char* title, bool* open)
	{
		return ImGui::Begin(title, open);
	}

	void End()
	{
		ImGui::End();
	}

	// --- Text ------------------------------------------------------------

	void Text(const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		ImGui::TextV(fmt, args);
		va_end(args);
	}

	void TextColored(const Color& color, const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		ImGui::TextColoredV(ToImVec4(color), fmt, args);
		va_end(args);
	}

	void TextWrapped(const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		ImGui::TextWrappedV(fmt, args);
		va_end(args);
	}

	void Label(const char* text)
	{
		ImGui::TextUnformatted(text);
	}

	// --- Widgets ---------------------------------------------------------

	bool Button(const char* label)
	{
		return ImGui::Button(label);
	}

	bool Checkbox(const char* label, bool* value)
	{
		return ImGui::Checkbox(label, value);
	}

	bool SliderFloat(const char* label, float* value, float minValue, float maxValue)
	{
		return ImGui::SliderFloat(label, value, minValue, maxValue);
	}

	bool SliderInt(const char* label, int* value, int minValue, int maxValue)
	{
		return ImGui::SliderInt(label, value, minValue, maxValue);
	}

	// --- Layout ----------------------------------------------------------

	void Separator()
	{
		ImGui::Separator();
	}

	void SameLine(float offsetFromStart, float spacing)
	{
		ImGui::SameLine(offsetFromStart, spacing);
	}

	void Spacing()
	{
		ImGui::Spacing();
	}

	void NewLine()
	{
		ImGui::NewLine();
	}

}
