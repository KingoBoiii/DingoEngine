#include "ClearColorTest.h"

namespace Dingo
{

	void ClearColorTest::Initialize()
	{
		CommandListParams commandListParams = CommandListParams()
			.SetTargetSwapChain(true);

		m_CommandList = CommandList::Create(commandListParams);
		m_CommandList->Initialize();
	}

	void ClearColorTest::Update(float deltaTime)
	{
		m_CommandList->Begin();
		m_CommandList->Clear();
		m_CommandList->End();
	}

	void ClearColorTest::Cleanup()
	{
		m_CommandList->Destroy();
	}

}
