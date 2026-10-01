#include <DingoEngine/EntryPoint.h>

#include "CandlewickLayer.h"

#include <charconv>
#include <optional>
#include <string_view>
#include <system_error>

namespace Dingo
{

	class CandlewickApplication : public Application
	{
	public:
		CandlewickApplication(const Dingo::ApplicationParams& params)
			: Application(params)
		{}
		virtual ~CandlewickApplication() = default;

	protected:
		virtual void OnInitialize() override
		{
			PushLayer(new CandlewickLayer());
		}
	};

}

static Dingo::GraphicsAPI ParseGraphicsAPI(const Dingo::ApplicationCommandLineArgs& args)
{
	if (auto val = args.Get("graphics"))
	{
		if (*val == "vulkan")  return Dingo::GraphicsAPI::Vulkan;
		if (*val == "dx11")    return Dingo::GraphicsAPI::DirectX11;
		if (*val == "dx12")    return Dingo::GraphicsAPI::DirectX12;
	}
	return Dingo::GraphicsAPI::Vulkan;
}

static uint32_t ParseLightBudget(const Dingo::ApplicationCommandLineArgs& args)
{
	const uint32_t fallback = Dingo::Renderer3DCapabilities{}.MaxLocalLights;
	const std::optional<std::string_view> value = args.Get("light-budget");
	if (!value)
		return fallback;

	uint32_t parsed = 0;
	const char* end = value->data() + value->size();
	const auto [ptr, error] = std::from_chars(value->data(), end, parsed);
	if (error != std::errc{} || ptr != end || parsed < 1 || parsed > Dingo::Renderer3D::k_MaxLocalLights)
	{
		DE_WARN("Candlewick: ignoring --light-budget={} (expected 1 to {})", *value, Dingo::Renderer3D::k_MaxLocalLights);
		return fallback;
	}
	return parsed;
}

Dingo::Application* Dingo::CreateApplication(Dingo::ApplicationCommandLineArgs args)
{
	ApplicationParams params = ApplicationParams{
		.CommandLineArgs = args,
		.Window = {
			.Title = "[Example] Candlewick (Point + Spot Lighting) - Dingo Engine",
			.Width = 1600,
			.Height = 900,
			.VSync = true,
			.Resizable = false,
		},
		.Graphics = {
			.GraphicsAPI = ParseGraphicsAPI(args),
			.FramesInFlight = 3,
		},
		.EnableUI = false,
	};
	params.Renderer3D.Capabilities.MaxLocalLights = ParseLightBudget(args);

	CandlewickApplication* app = new CandlewickApplication(params);
	app->Initialize();
	return app;
}
