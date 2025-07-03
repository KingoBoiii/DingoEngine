#pragma once
#include "DingoEngine/Log.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include <nvrhi/nvrhi.h>

namespace DingoEngine::Utils
{

	static void WriteBuffer(nvrhi::CommandListHandle& commandList, nvrhi::BufferHandle buffer, const void* data, uint64_t size, uint64_t offset = 0ul)
	{
		if (data == nullptr || size == 0)
		{
			DE_CORE_WARN("WriteBuffer called with null data or zero size.");
			return;
		}

		//commandList->open();

		commandList->writeBuffer(buffer, data, size, offset);

		//commandList->close();

		//GraphicsContext::GetDeviceHandle()->executeCommandList(commandList);
		//GraphicsContext::GetDeviceHandle()->waitForIdle();
		//GraphicsContext::GetDeviceHandle()->runGarbageCollection();
	}

}
