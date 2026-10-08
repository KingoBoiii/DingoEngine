#include <DingoEngine/EntryPoint.h>
#include <DingoEngine/Core/Platform.h>

#include "CandlewickLayer.h"
#include "GameTuning.h"

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

static bool ParseVSync(const Dingo::ApplicationCommandLineArgs& args)
{
	if (const std::optional<std::string_view> value = args.Get("vsync"))
	{
		if (*value == "off" || *value == "0" || *value == "false")
			return false;
		if (!value->empty() && *value != "on" && *value != "1" && *value != "true")
			DE_WARN("Candlewick: ignoring --vsync={} (expected on/1/true or off/0/false)", *value);
	}
	return true;
}

static bool IsFlagOn(const Dingo::ApplicationCommandLineArgs& args, std::string_view name)
{
	const std::optional<std::string_view> value = args.Get(name);
	return value && (value->empty() || *value == "1" || *value == "true" || *value == "on");
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
	if (error != std::errc{} || ptr != end || parsed < static_cast<uint32_t>(Dingo::LIGHT_BUDGET_MIN) || parsed > Dingo::Renderer3D::k_MaxLocalLights)
	{
		DE_WARN("Candlewick: ignoring --light-budget={} (expected {} to {}: the {} gameplay lights plus the light LOD's headroom must always fit)", *value,
			Dingo::LIGHT_BUDGET_MIN, Dingo::Renderer3D::k_MaxLocalLights, Dingo::GAMEPLAY_LIGHTS_MAX);
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
			.VSync = ParseVSync(args),
			.Resizable = false,
		},
		.Graphics = {
			.GraphicsAPI = ParseGraphicsAPI(args),
			.FramesInFlight = 3,
		},
		.Assets = AssetManagerParams()
			.SetRootDirectory(Platform::FindDirectoryUpward("assets").value_or("assets")),
		.EnableUI = false,
	};
	params.Renderer3D.Capabilities.MaxLocalLights = ParseLightBudget(args);
	// Every gameplay light casts and nothing else does, so all of them always hold a shadow slot.
	static_assert(Dingo::GAMEPLAY_LIGHTS_MAX <= static_cast<int>(Dingo::Renderer3D::k_MaxShadowedLocalLights));
	params.Renderer3D.Capabilities.MaxShadowedLocalLights = Dingo::Renderer3D::k_MaxShadowedLocalLights;
	// A scripted run must not pause when its window opens unfocused.
	params.UpdateInBackground = IsFlagOn(args, "perf") || IsFlagOn(args, "hide-check");

	CandlewickApplication* app = new CandlewickApplication(params);
	app->Initialize();
	return app;
}
