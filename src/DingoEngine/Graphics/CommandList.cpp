#include "depch.h"
#include "DingoEngine/Graphics/CommandList.h"

#include "DingoEngine/Graphics/NVRHI/NvrhiCommandList.h"

namespace Dingo
{

	Internal::CommandList* Internal::CommandList::Create(const CommandListParams& params)
	{
		Internal::CommandList* commandList = new NvrhiCommandList(params);
		commandList->Initialize();
		return commandList;
	}

}

