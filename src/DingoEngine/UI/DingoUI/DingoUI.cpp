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
