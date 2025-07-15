#include "depch.h"
#include "NvrhiGraphicsContext.h"

namespace Dingo
{

	void NvrhiGraphicsContext::RunGarbageCollection() const
	{
		m_DeviceHandle->runGarbageCollection();
	}

}
