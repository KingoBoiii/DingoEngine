#include "depch.h"
#include "DingoEngine/UI/DingoUI.h"

#include "DingoUIContext.h"

namespace Dingo
{

	bool DingoUI::BeginRoot(std::string_view name, const UIRootParams& params)
	{
		Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		return context && context->BeginRoot(name, params);
	}

	void DingoUI::EndRoot()
	{
		if (Internal::DingoUIContext* context = Internal::GetDingoUIContext())
			context->EndRoot();
	}

	bool DingoUI::BeginBox(const UIBoxParams& params)
	{
		Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		return context && context->BeginBox(params);
	}

	void DingoUI::EndBox()
	{
		if (Internal::DingoUIContext* context = Internal::GetDingoUIContext())
			context->EndBox();
	}

	void DingoUI::Text(std::string_view text, const UITextParams& params)
	{
		if (Internal::DingoUIContext* context = Internal::GetDingoUIContext())
			context->Text(text, params);
	}

	void DingoUI::Image(Texture* texture, const UIImageParams& params)
	{
		if (Internal::DingoUIContext* context = Internal::GetDingoUIContext())
			context->Image(texture, params);
	}

	UIInteraction DingoUI::Button(std::string_view label, const UIWidgetParams& params)
	{
		Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		return context ? context->Button(label, params) : UIInteraction{};
	}

	UIInteraction DingoUI::Toggle(std::string_view label, bool* value, const UIWidgetParams& params)
	{
		Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		return context ? context->Toggle(label, value, params) : UIInteraction{};
	}

	UIInteraction DingoUI::Slider(std::string_view label, float* value, float min, float max, const UISliderParams& params)
	{
		Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		return context ? context->Slider(label, value, min, max, params) : UIInteraction{};
	}

	UIInteraction DingoUI::SliderInt(std::string_view label, int* value, int min, int max, const UISliderParams& params)
	{
		Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		return context ? context->SliderInt(label, value, min, max, params) : UIInteraction{};
	}

	void DingoUI::ProgressBar(std::string_view id, float value, const UIWidgetParams& params)
	{
		if (Internal::DingoUIContext* context = Internal::GetDingoUIContext())
			context->ProgressBar(id, value, params);
	}

	UIInteraction DingoUI::Selectable(std::string_view label, bool selected, const UIWidgetParams& params)
	{
		Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		return context ? context->Selectable(label, selected, params) : UIInteraction{};
	}

	bool DingoUI::BeginScrollArea(std::string_view label, const UIScrollParams& params)
	{
		Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		return context && context->BeginScrollArea(label, params);
	}

	void DingoUI::EndScrollArea()
	{
		if (Internal::DingoUIContext* context = Internal::GetDingoUIContext())
			context->EndScrollArea();
	}

	bool DingoUI::BeginPanel(const UIBoxParams& params)
	{
		Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		return context && context->BeginPanel(params);
	}

	void DingoUI::EndPanel()
	{
		if (Internal::DingoUIContext* context = Internal::GetDingoUIContext())
			context->EndPanel();
	}

	void DingoUI::PushStyle(const UIStyle& style)
	{
		if (Internal::DingoUIContext* context = Internal::GetDingoUIContext())
			context->PushStyle(style);
	}

	void DingoUI::PopStyle()
	{
		if (Internal::DingoUIContext* context = Internal::GetDingoUIContext())
			context->PopStyle();
	}

	const UIStyle& DingoUI::GetStyle()
	{
		static const UIStyle s_Fallback;
		const Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		return context ? context->GetStyle() : s_Fallback;
	}

	void DingoUI::SetTheme(const UIStyle& style)
	{
		if (Internal::DingoUIContext* context = Internal::GetDingoUIContext())
			context->SetTheme(style);
	}

	void DingoUI::PushId(std::string_view id)
	{
		if (Internal::DingoUIContext* context = Internal::GetDingoUIContext())
			context->PushId(id);
	}

	void DingoUI::PushId(int id)
	{
		if (Internal::DingoUIContext* context = Internal::GetDingoUIContext())
			context->PushId(id);
	}

	void DingoUI::PopId()
	{
		if (Internal::DingoUIContext* context = Internal::GetDingoUIContext())
			context->PopId();
	}

	void DingoUI::SetDefaultFont(Font* font)
	{
		if (Internal::DingoUIContext* context = Internal::GetDingoUIContext())
			context->SetDefaultFont(font);
	}

	Font* DingoUI::GetDefaultFont()
	{
		const Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		return context ? context->GetDefaultFont() : nullptr;
	}

	bool DingoUI::IsPointerOverUI()
	{
		const Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		return context && context->IsPointerOverUI();
	}

	bool DingoUI::IsCapturingInput()
	{
		const Internal::DingoUIContext* context = Internal::GetDingoUIContext();
		return context && context->IsCapturingInput();
	}

}
