#include "depch.h"
#include "DingoEngine/Core/XInput.h"

#ifdef DE_PLATFORM_WINDOWS
#include <Xinput.h>
#endif

#include <algorithm>

namespace Dingo::Internal::XInput
{

#ifdef DE_PLATFORM_WINDOWS

	namespace
	{
		using GetCapabilitiesFn = DWORD(WINAPI*)(DWORD, DWORD, XINPUT_CAPABILITIES*);
		using SetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_VIBRATION*);

		struct Api
		{
			GetCapabilitiesFn GetCapabilities = nullptr;
			SetStateFn SetState = nullptr;
		};

		// Loaded at run time, as GLFW loads it, so the engine links no XInput library and starts
		// on a system without one.
		const Api& GetApi()
		{
			static const Api s_Api = []()
			{
				Api api;
				for (const wchar_t* name : { L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll" })
				{
					if (HMODULE module = LoadLibraryW(name))
					{
						api.GetCapabilities = reinterpret_cast<GetCapabilitiesFn>(GetProcAddress(module, "XInputGetCapabilities"));
						api.SetState = reinterpret_cast<SetStateFn>(GetProcAddress(module, "XInputSetState"));
						break;
					}
				}
				return api;
			}();
			return s_Api;
		}

		WORD ToMotorSpeed(float speed)
		{
			if (!(speed > 0.0f))
				return 0;
			return static_cast<WORD>(std::min(speed, 1.0f) * 65535.0f + 0.5f);
		}
	}

	uint32_t GetConnectedMask()
	{
		const Api& api = GetApi();
		if (!api.GetCapabilities)
			return 0;

		uint32_t mask = 0;
		for (DWORD user = 0; user < k_MaxUsers; ++user)
		{
			XINPUT_CAPABILITIES capabilities{};
			if (api.GetCapabilities(user, 0, &capabilities) == ERROR_SUCCESS)
				mask |= 1u << user;
		}
		return mask;
	}

	bool SetVibration(uint32_t userIndex, float lowFrequency, float highFrequency)
	{
		const Api& api = GetApi();
		if (!api.SetState || userIndex >= k_MaxUsers)
			return false;

		XINPUT_VIBRATION vibration{ ToMotorSpeed(lowFrequency), ToMotorSpeed(highFrequency) };
		return api.SetState(userIndex, &vibration) == ERROR_SUCCESS;
	}

#else

	uint32_t GetConnectedMask()
	{
		return 0;
	}

	bool SetVibration(uint32_t, float, float)
	{
		return false;
	}

#endif

}
