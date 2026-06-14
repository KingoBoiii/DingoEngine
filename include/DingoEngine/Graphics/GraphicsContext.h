#pragma once
#include "GraphicsParams.h"
#include "DingoEngine/Common.h"

#include <string>
#include <cstdint>

struct GLFWwindow;

namespace Dingo
{

	enum class AdapterDeviceType
	{
		Unknown,
		Discrete,
		Integrated,
		Virtual,
		Software
	};

	struct AdapterInfo
	{
		std::string Name;
		uint32_t VendorID = 0;
		uint32_t DeviceID = 0;
		uint64_t DedicatedVideoMemory = 0;  // bytes
		AdapterDeviceType DeviceType = AdapterDeviceType::Unknown;
	};

	class GraphicsContext
	{
	public:
		static GraphicsContext* Create(const GraphicsParams& params);

	public:
		GraphicsContext(const GraphicsParams& params);
		virtual ~GraphicsContext();

	public:
		virtual void Initialize() = 0;
		virtual void Shutdown() = 0;

		virtual void RunGarbageCollection() const = 0;

		const GraphicsParams& GetParams() { return m_Params; }
		const GraphicsParams& GetParams() const { return m_Params; }
		GraphicsAPI GetGraphicsAPI() const { return m_Params.GraphicsAPI; }
		const AdapterInfo& GetAdapterInfo() const { return m_AdapterInfo; }
		static GraphicsContext& Get() { return *s_Instance; }

		static std::string VendorName(uint32_t vendorID);

		template<typename T>
		T& As() { return static_cast<T&>(*this); }

	protected:
		GraphicsParams m_Params;
		AdapterInfo m_AdapterInfo;

	private:
		inline static GraphicsContext* s_Instance = nullptr;
	};

}
