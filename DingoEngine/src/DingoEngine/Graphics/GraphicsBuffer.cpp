#include "depch.h"
#include "DingoEngine/Graphics/GraphicsBuffer.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include "NVRHI/NvrhiGraphicsBuffer.h"

namespace Dingo
{

	GraphicsBuffer* GraphicsBuffer::Create(const GraphicsBufferParams& params)
	{
		return new NvrhiGraphicsBuffer(params);
	}

}
