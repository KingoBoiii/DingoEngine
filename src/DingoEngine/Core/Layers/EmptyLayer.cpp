#include "depch.h"
#include "EmptyLayer.h"

namespace Dingo
{

	void EmptyLayer::OnAttach()
	{
		CommandListParams commandListParams = CommandListParams()
			.SetTargetSwapChain(true);

		m_CommandList = CommandList::Create(commandListParams);
		m_CommandList->Initialize();
	}

	void EmptyLayer::OnDetach()
	{
		m_CommandList->Destroy();
	}

	void EmptyLayer::OnUpdate()
	{
		m_CommandList->Begin();
		m_CommandList->Clear();
		m_CommandList->End();
	}

}
