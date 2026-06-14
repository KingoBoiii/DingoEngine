#include "depch.h"
#include "NvrhiGraphicsContext.h"

namespace Dingo
{

	void NvrhiMessageCallback::message(nvrhi::MessageSeverity severity, const char* messageText)
	{
		switch (severity)
		{
			case nvrhi::MessageSeverity::Info:    DE_CORE_INFO("[NVRHI] {}", messageText); break;
			case nvrhi::MessageSeverity::Warning: DE_CORE_WARN("[NVRHI] {}", messageText); break;
			case nvrhi::MessageSeverity::Error:   DE_CORE_ERROR("[NVRHI] {}", messageText); break;
			case nvrhi::MessageSeverity::Fatal:   DE_CORE_ERROR("[NVRHI] FATAL: {}", messageText); DE_CORE_ASSERT(false); break;
		}
	}

	void NvrhiGraphicsContext::RunGarbageCollection() const
	{
		m_DeviceHandle->runGarbageCollection();
	}

}
