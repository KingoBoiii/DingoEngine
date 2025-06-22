#pragma once
#include "DingoEngine/Common.h"

#include <string>
#include <nvrhi/nvrhi.h>

struct GLFWwindow;

namespace DingoEngine
{

	class GraphicsContext
	{
	public:
		static GraphicsContext* Create(GraphicsAPI graphicsAPI);

	public:
		GraphicsContext(GraphicsAPI graphicsAPI);
		virtual ~GraphicsContext();

	public:
		virtual void Initialize() = 0;
		virtual void Shutdown() = 0;

		void RenderStatic() const;

		static GraphicsAPI GetApi()
		{
			return s_Instance->m_GraphicsAPI;
		}

		static nvrhi::DeviceHandle GetDeviceHandle()
		{
			return s_Instance->m_DeviceHandle;
		}

		static GraphicsContext& Get()
		{
			return *s_Instance;
		}

	protected:
		nvrhi::DeviceHandle m_DeviceHandle;
		GraphicsAPI m_GraphicsAPI;

	private:
		inline static GraphicsContext* s_Instance = nullptr;
	};

}
