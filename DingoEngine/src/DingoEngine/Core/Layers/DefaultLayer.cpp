#include "depch.h"
#include "DefaultLayer.h"

namespace DingoEngine
{

	void DefaultLayer::OnAttach()
	{
		CommandListParams commandListParams = CommandListParams()
			.SetTargetSwapChain(true);

		m_CommandList = CommandList::Create(commandListParams);
		m_CommandList->Initialize();
	}

	void DefaultLayer::OnDetach()
	{
		m_CommandList->Destroy();
	}

	void DefaultLayer::OnUpdate()
	{
		m_CommandList->Begin();
		m_CommandList->Clear();
		m_CommandList->End();
	}

}
