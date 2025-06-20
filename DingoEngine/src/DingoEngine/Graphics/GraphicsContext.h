#pragma once
#include "DingoEngine/Common.h"

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
		void Initialize();
		void Shutdown();

	protected:
		virtual nvrhi::DeviceHandle CreateDeviceHandle() = 0;
		virtual void DestroyDeviceHandle(nvrhi::DeviceHandle deviceHandle) = 0;

	private:
		nvrhi::DeviceHandle m_DeviceHandle;
		GraphicsAPI m_GraphicsAPI;
	};

}
