#include "depch.h"
#include "DingoEngine/Graphics/CommandList.h"

#include "DingoEngine/Graphics/NVRHI/NvrhiCommandList.h"

namespace Dingo
{

	CommandList* CommandList::Create(const CommandListParams& params)
	{
		return new NvrhiCommandList(params);
	}

}

