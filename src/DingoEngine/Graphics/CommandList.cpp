#include "depch.h"
#include "DingoEngine/Graphics/CommandList.h"

#include "DingoEngine/Graphics/NVRHI/NvrhiCommandList.h"

namespace Dingo
{

	CommandList* CommandList::Create(const CommandListParams& params)
	{
		CommandList* commandList = new NvrhiCommandList(params);
		commandList->Initialize();
		return commandList;
	}

}

