#include "depch.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include "DingoEngine/Graphics/NVRHI/Vulkan/VulkanGraphicsContext.h"

#ifdef DE_PLATFORM_WINDOWS
#include "DingoEngine/Graphics/NVRHI/DirectX11/DirectX11GraphicsContext.h"
#include "DingoEngine/Graphics/NVRHI/DirectX12/DirectX12GraphicsContext.h"
#endif

namespace Dingo
{

	GraphicsContext* GraphicsContext::Create(const GraphicsParams& params)
	{
		switch (params.GraphicsAPI)
		{
			case GraphicsAPI::Vulkan: return new VulkanGraphicsContext(params);
#ifdef DE_PLATFORM_WINDOWS
			case GraphicsAPI::DirectX11: return new DirectX11GraphicsContext(params);
			case GraphicsAPI::DirectX12: return new DirectX12GraphicsContext(params);
#endif
			default: break;
		}
		DE_CORE_ASSERT(false, "Unsupported or unavailable GraphicsAPI on this platform.");
		return nullptr;
	}

	GraphicsContext::GraphicsContext(const GraphicsParams& params)
		: m_Params(params)
	{
		s_Instance = this;
	}

	GraphicsContext::~GraphicsContext()
	{
		s_Instance = nullptr;
	}

}
