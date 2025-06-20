#pragma once
#include "DingoEngine/Common.h"

#include <string>
#include <nvrhi/nvrhi.h>

namespace DingoEngine
{

	class GraphicsContext
	{
	public:
		static GraphicsContext* Create(GraphicsAPI graphicsAPI);

	public:
		GraphicsContext(GraphicsAPI graphicsAPI);
		virtual ~GraphicsContext() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Shutdown() = 0;

	protected:
		nvrhi::DeviceHandle m_DeviceHandler;
		GraphicsAPI m_GraphicsAPI;
	};

}
