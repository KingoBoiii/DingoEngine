#pragma once
#include "DingoEngine/Graphics/GraphicsContext.h"

#include <nvrhi/nvrhi.h>

namespace Dingo
{

	class NvrhiGraphicsContext : public GraphicsContext
	{
	public:
		NvrhiGraphicsContext(const GraphicsParams& params)
			: GraphicsContext(params)
		{}
		virtual ~NvrhiGraphicsContext() = default;

	public:
		virtual void RunGarbageCollection() const override;

		nvrhi::DeviceHandle GetDeviceHandle() const { return m_DeviceHandle; }

	protected:
		nvrhi::DeviceHandle m_DeviceHandle;
	};

}
