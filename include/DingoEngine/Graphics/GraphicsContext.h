#pragma once
#include "GraphicsParams.h"
#include "DingoEngine/Common.h"

#include <string>
#include <cstdint>

struct GLFWwindow;

namespace Dingo
{

	/// What kind of device a graphics adapter is.
	enum class AdapterDeviceType
	{
		Unknown,
		Discrete,
		Integrated,
		Virtual,
		Software	///< A CPU rasteriser such as llvmpipe or WARP.
	};

	/// The GPU the context picked, for display and diagnostics.
	struct AdapterInfo
	{
		std::string Name;	///< The driver's device name.
		uint32_t VendorID = 0;	///< PCI vendor id; see GraphicsContext::VendorName.
		uint32_t DeviceID = 0;	///< PCI device id.
		uint64_t DedicatedVideoMemory = 0;  ///< In bytes.
		AdapterDeviceType DeviceType = AdapterDeviceType::Unknown;	///< Discrete, integrated, ...
	};

	/// The graphics device and API backend (Vulkan, DirectX 11 or DirectX 12) the Application
	/// created. There is exactly one, owned by the Application; read it, never create or delete it.
	class GraphicsContext
	{
	public:
		/// Releases the device; only the Application deletes the context.
		virtual ~GraphicsContext();

		/// The params the application started the context with.
		///@{
		const GraphicsParams& GetParams() { return m_Params; }
		const GraphicsParams& GetParams() const { return m_Params; }
		///@}
		/// The API in use, as chosen in the GraphicsParams.
		GraphicsAPI GetGraphicsAPI() const { return m_Params.GraphicsAPI; }
		/// The GPU the context selected.
		const AdapterInfo& GetAdapterInfo() const { return m_AdapterInfo; }
		/// False on a Vulkan GPU without fillModeNonSolid, where a FillMode::Wireframe pipeline is
		/// built solid instead.
		bool SupportsWireframe() const { return m_SupportsWireframe; }
		/// False on a Vulkan GPU without shaderClipDistance, which the shadow pass needs to keep
		/// each view inside its atlas tile; Renderer3D then draws no shadows.
		bool SupportsClipDistance() const { return m_SupportsClipDistance; }
		/// The one live context; valid from the Application's start-up until its shutdown.
		static GraphicsContext& Get() { return *s_Instance; }

		/// A readable vendor name for a PCI vendor id ("NVIDIA", "AMD", "Intel", ...), or
		/// "Unknown".
		static std::string VendorName(uint32_t vendorID);

		/// Casts the context to its backend type; the caller must know which backend is running.
		template<typename T>
		T& As() { return static_cast<T&>(*this); }

	protected:
		/// Stores the params and registers this as the one context (Get()).
		GraphicsContext(const GraphicsParams& params);

		/// Brings up the device for the chosen API; called by the Application.
		virtual void Initialize() = 0;
		/// Tears the device down; called by the Application once nothing uses it.
		virtual void Shutdown() = 0;

		GraphicsParams m_Params;	///< The params the context was created with.
		/// The application window, so device selection can check which GPU can present to the
		/// display it lives on. Null = selection is surface-blind.
		GLFWwindow* m_Window = nullptr;
		AdapterInfo m_AdapterInfo;	///< Filled by Initialize().
		bool m_SupportsWireframe = true;	///< Backs SupportsWireframe().
		bool m_SupportsClipDistance = true;	///< Backs SupportsClipDistance().

	private:
		// The Application owns the one context (GetGraphicsContext) and drives it.
		static GraphicsContext* Create(const GraphicsParams& params, GLFWwindow* window);
		virtual void RunGarbageCollection() const = 0;

		inline static GraphicsContext* s_Instance = nullptr;

		friend class Application;
		friend class Renderer;
	};

}
