#pragma once
#include "DingoEngine/Graphics/GraphicsContext.h"

#include <nvrhi/nvrhi.h>

namespace Dingo
{

	class NvrhiMessageCallback : public nvrhi::IMessageCallback
	{
	public:
		static NvrhiMessageCallback& Get()
		{
			static NvrhiMessageCallback s_Instance;
			return s_Instance;
		}

		virtual void message(nvrhi::MessageSeverity severity, const char* messageText) override;
	};

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
