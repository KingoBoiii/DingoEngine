#include "depch.h"
#include "DingoEngine/Core/PadRumble.h"

#if defined(DE_PLATFORM_WINDOWS)
#include <SetupAPI.h>
#include <cwchar>
#include <cwctype>
#elif defined(DE_PLATFORM_LINUX)
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <climits>
#include <cstring>

namespace Dingo::Internal::PadRumble
{

	namespace
	{
		uint8_t ToMotorByte(float speed)
		{
			if (!(speed > 0.0f))
				return 0;
			return static_cast<uint8_t>((std::min)(speed, 1.0f) * 255.0f + 0.5f);
		}

		int HexDigit(char c)
		{
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			return -1;
		}
	}

	DeviceIds ParseGlfwGuid(const char* guid)
	{
		if (!guid || std::strlen(guid) != 32)
			return {};

		uint8_t bytes[16]{};
		for (size_t i = 0; i < 16; ++i)
		{
			const int high = HexDigit(guid[i * 2]);
			const int low = HexDigit(guid[i * 2 + 1]);
			if (high < 0 || low < 0)
				return {};
			bytes[i] = static_cast<uint8_t>((high << 4) | low);
		}

		auto field = [&](size_t index) { return static_cast<uint16_t>(bytes[index] | (bytes[index + 1] << 8)); };
		if (field(2) != 0 || field(6) != 0 || field(10) != 0 || field(14) != 0 || field(4) == 0 || field(8) == 0)
			return {};
		return { field(0), field(4), field(8), field(12) };
	}

#if defined(DE_PLATFORM_WINDOWS)

	namespace
	{
		constexpr uint16_t k_SonyVendor = 0x054c;

		enum class PadKind { DualShock4, DualSense };

		struct SonyPad
		{
			uint16_t Product;
			PadKind Kind;
		};

		constexpr SonyPad k_SonyPads[] = {
			{ 0x05c4, PadKind::DualShock4 },
			{ 0x09cc, PadKind::DualShock4 },
			{ 0x0ba0, PadKind::DualShock4 }, // the USB wireless adapter
			{ 0x0ce6, PadKind::DualSense },
			{ 0x0df2, PadKind::DualSense },  // Edge
		};
		constexpr uint16_t k_DualSenseEdge = 0x0df2;

		// The USB output reports' sizes, report id included: the fields need this many bytes, and
		// Windows takes writes of the full OutputReportByteLength, which is at most a USB packet. A pad
		// on Bluetooth reports a longer one and is left alone: its first output report switches it to
		// extended input reports, which DirectInput (and so GLFW) can't read.
		constexpr ULONG k_DualShock4UsbReportSize = 32;
		constexpr ULONG k_DualSenseUsbReportSize = 48;
		constexpr ULONG k_MaxUsbReportSize = 64;

		// hidsdi.h and hidpi.h come with the WDK's types; these match their layouts.
		struct HidAttributes
		{
			ULONG Size;
			USHORT VendorID;
			USHORT ProductID;
			USHORT VersionNumber;
		};

		struct HidCaps
		{
			USHORT Usage;
			USHORT UsagePage;
			USHORT InputReportByteLength;
			USHORT OutputReportByteLength;
			USHORT FeatureReportByteLength;
			USHORT Reserved[17];
			USHORT NumberLinkCollectionNodes;
			USHORT NumberInputButtonCaps;
			USHORT NumberInputValueCaps;
			USHORT NumberInputDataIndices;
			USHORT NumberOutputButtonCaps;
			USHORT NumberOutputValueCaps;
			USHORT NumberOutputDataIndices;
			USHORT NumberFeatureButtonCaps;
			USHORT NumberFeatureValueCaps;
			USHORT NumberFeatureDataIndices;
		};

		constexpr LONG k_HidpStatusSuccess = 0x00110000;

		using HidGetHidGuidFn = void(WINAPI*)(GUID*);
		using HidGetAttributesFn = BOOLEAN(WINAPI*)(HANDLE, HidAttributes*);
		using HidGetPreparsedDataFn = BOOLEAN(WINAPI*)(HANDLE, void**);
		using HidFreePreparsedDataFn = BOOLEAN(WINAPI*)(void*);
		using HidGetCapsFn = LONG(WINAPI*)(void*, HidCaps*);
		using HidGetFeatureFn = BOOLEAN(WINAPI*)(HANDLE, void*, ULONG);
		using GetClassDevsFn = HDEVINFO(WINAPI*)(const GUID*, PCWSTR, HWND, DWORD);
		using EnumDeviceInterfacesFn = BOOL(WINAPI*)(HDEVINFO, PSP_DEVINFO_DATA, const GUID*, DWORD, PSP_DEVICE_INTERFACE_DATA);
		using GetDeviceInterfaceDetailFn = BOOL(WINAPI*)(HDEVINFO, PSP_DEVICE_INTERFACE_DATA, PSP_DEVICE_INTERFACE_DETAIL_DATA_W, DWORD, PDWORD, PSP_DEVINFO_DATA);
		using DestroyDeviceInfoListFn = BOOL(WINAPI*)(HDEVINFO);

		struct Api
		{
			HidGetHidGuidFn GetHidGuid = nullptr;
			HidGetAttributesFn GetAttributes = nullptr;
			HidGetPreparsedDataFn GetPreparsedData = nullptr;
			HidFreePreparsedDataFn FreePreparsedData = nullptr;
			HidGetCapsFn GetCaps = nullptr;
			HidGetFeatureFn GetFeature = nullptr;
			GetClassDevsFn GetClassDevs = nullptr;
			EnumDeviceInterfacesFn EnumDeviceInterfaces = nullptr;
			GetDeviceInterfaceDetailFn GetDeviceInterfaceDetail = nullptr;
			DestroyDeviceInfoListFn DestroyDeviceInfoList = nullptr;

			bool Loaded() const
			{
				return GetHidGuid && GetAttributes && GetPreparsedData && FreePreparsedData && GetCaps && GetFeature
					&& GetClassDevs && EnumDeviceInterfaces && GetDeviceInterfaceDetail && DestroyDeviceInfoList;
			}
		};

		// Loaded at run time, like XInput, so the engine's consumers link no extra system libraries.
		const Api& GetApi()
		{
			static const Api s_Api = []()
			{
				Api api;
				HMODULE hid = LoadLibraryW(L"hid.dll");
				HMODULE setupApi = LoadLibraryW(L"setupapi.dll");
				if (!hid || !setupApi)
					return api;

				api.GetHidGuid = reinterpret_cast<HidGetHidGuidFn>(GetProcAddress(hid, "HidD_GetHidGuid"));
				api.GetAttributes = reinterpret_cast<HidGetAttributesFn>(GetProcAddress(hid, "HidD_GetAttributes"));
				api.GetPreparsedData = reinterpret_cast<HidGetPreparsedDataFn>(GetProcAddress(hid, "HidD_GetPreparsedData"));
				api.FreePreparsedData = reinterpret_cast<HidFreePreparsedDataFn>(GetProcAddress(hid, "HidD_FreePreparsedData"));
				api.GetCaps = reinterpret_cast<HidGetCapsFn>(GetProcAddress(hid, "HidP_GetCaps"));
				api.GetFeature = reinterpret_cast<HidGetFeatureFn>(GetProcAddress(hid, "HidD_GetFeature"));
				api.GetClassDevs = reinterpret_cast<GetClassDevsFn>(GetProcAddress(setupApi, "SetupDiGetClassDevsW"));
				api.EnumDeviceInterfaces = reinterpret_cast<EnumDeviceInterfacesFn>(GetProcAddress(setupApi, "SetupDiEnumDeviceInterfaces"));
				api.GetDeviceInterfaceDetail = reinterpret_cast<GetDeviceInterfaceDetailFn>(GetProcAddress(setupApi, "SetupDiGetDeviceInterfaceDetailW"));
				api.DestroyDeviceInfoList = reinterpret_cast<DestroyDeviceInfoListFn>(GetProcAddress(setupApi, "SetupDiDestroyDeviceInfoList"));
				return api;
			}();
			return s_Api;
		}

		struct Device
		{
			HANDLE Handle = INVALID_HANDLE_VALUE;
			std::wstring Path;
			PadKind Kind = PadKind::DualSense;
			ULONG ReportSize = 0;
			int LastLow = -1;
			int LastHigh = -1;
			// Firmware 2.24 and newer have a better rumble emulation behind its own flag.
			bool ImprovedRumble = false;
		};

		std::array<Device, k_MaxJoysticks> s_Devices{};

		const SonyPad* FindSonyPad(uint16_t vendor, uint16_t product)
		{
			if (vendor != k_SonyVendor)
				return nullptr;
			for (const SonyPad& pad : k_SonyPads)
			{
				if (pad.Product == product)
					return &pad;
			}
			return nullptr;
		}

		bool HeldByAnother(const std::wstring& path)
		{
			return std::any_of(s_Devices.begin(), s_Devices.end(), [&](const Device& device) { return device.Handle != INVALID_HANDLE_VALUE && device.Path == path; });
		}

		// Keyboards and security keys stay unopened: only a path naming the pad's ids is tried.
		bool PathNamesIds(const std::wstring& path, uint16_t vendor, uint16_t product)
		{
			wchar_t ids[32]{};
			swprintf(ids, 32, L"vid_%04x&pid_%04x", vendor, product);
			std::wstring lower = path;
			std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
			return lower.find(ids) != std::wstring::npos;
		}

		HANDLE OpenHid(const wchar_t* path)
		{
			return CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
		}

		bool ReadCaps(const Api& api, HANDLE handle, HidCaps& caps)
		{
			void* preparsed = nullptr;
			if (!api.GetPreparsedData(handle, &preparsed))
				return false;
			const bool ok = api.GetCaps(preparsed, &caps) == k_HidpStatusSuccess;
			api.FreePreparsedData(preparsed);
			return ok;
		}

		bool HasImprovedRumble(const Api& api, HANDLE handle, uint16_t product, const HidCaps& caps)
		{
			if (product == k_DualSenseEdge)
				return true;

			// Feature report 0x20 holds the firmware version at bytes 44 and 45. A pad that won't
			// say is taken to be up to date, as SDL does.
			constexpr uint8_t k_FirmwareInfoReport = 0x20;
			std::vector<uint8_t> report((std::max)(static_cast<size_t>(caps.FeatureReportByteLength), size_t{ 64 }), 0);
			report[0] = k_FirmwareInfoReport;
			if (!api.GetFeature(handle, report.data(), static_cast<ULONG>(report.size())))
				return true;
			const uint16_t version = static_cast<uint16_t>(report[44] | (report[45] << 8));
			return version == 0 || version >= 0x0224;
		}

		bool WriteReport(const Device& device, const std::vector<uint8_t>& report)
		{
			OVERLAPPED overlapped{};
			overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
			if (!overlapped.hEvent)
				return false;

			DWORD written = 0;
			bool ok = WriteFile(device.Handle, report.data(), static_cast<DWORD>(report.size()), &written, &overlapped) != FALSE;
			if (!ok && GetLastError() == ERROR_IO_PENDING)
			{
				// A USB report goes out within a frame; a pad that stops answering mustn't stall the game.
				if (WaitForSingleObject(overlapped.hEvent, 50) != WAIT_OBJECT_0)
					CancelIoEx(device.Handle, &overlapped);
				ok = GetOverlappedResult(device.Handle, &overlapped, &written, TRUE) != FALSE;
			}
			CloseHandle(overlapped.hEvent);
			return ok && written == report.size();
		}
	}

	bool Pair(uint32_t joystick, const DeviceIds& ids, const char*)
	{
		if (joystick >= k_MaxJoysticks)
			return false;
		if (s_Devices[joystick].Handle != INVALID_HANDLE_VALUE)
			return true;

		const SonyPad* wanted = FindSonyPad(ids.Vendor, ids.Product);
		const Api& api = GetApi();
		if (!wanted || !api.Loaded())
			return false;

		GUID hidGuid{};
		api.GetHidGuid(&hidGuid);
		HDEVINFO deviceInfo = api.GetClassDevs(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
		if (deviceInfo == INVALID_HANDLE_VALUE)
			return false;

		bool paired = false;
		SP_DEVICE_INTERFACE_DATA interfaceData{};
		interfaceData.cbSize = sizeof(interfaceData);
		for (DWORD index = 0; !paired && api.EnumDeviceInterfaces(deviceInfo, nullptr, &hidGuid, index, &interfaceData); ++index)
		{
			DWORD required = 0;
			api.GetDeviceInterfaceDetail(deviceInfo, &interfaceData, nullptr, 0, &required, nullptr);
			if (required < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
				continue;

			std::vector<uint8_t> detailBuffer(required, 0);
			auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detailBuffer.data());
			detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
			if (!api.GetDeviceInterfaceDetail(deviceInfo, &interfaceData, detail, required, nullptr, nullptr))
				continue;

			const std::wstring path = detail->DevicePath;
			if (HeldByAnother(path) || !PathNamesIds(path, ids.Vendor, ids.Product))
				continue;

			HANDLE handle = OpenHid(path.c_str());
			if (handle == INVALID_HANDLE_VALUE)
				continue;

			HidAttributes attributes{};
			attributes.Size = sizeof(attributes);
			HidCaps caps{};
			const ULONG usbSize = wanted->Kind == PadKind::DualSense ? k_DualSenseUsbReportSize : k_DualShock4UsbReportSize;
			if (api.GetAttributes(handle, &attributes) && attributes.VendorID == ids.Vendor && attributes.ProductID == ids.Product
				&& ReadCaps(api, handle, caps) && caps.OutputReportByteLength >= usbSize && caps.OutputReportByteLength <= k_MaxUsbReportSize)
			{
				Device& device = s_Devices[joystick];
				device.Handle = handle;
				device.Path = path;
				device.Kind = wanted->Kind;
				device.ReportSize = caps.OutputReportByteLength;
				device.ImprovedRumble = wanted->Kind == PadKind::DualSense && HasImprovedRumble(api, handle, ids.Product, caps);
				paired = true;
			}
			else
			{
				CloseHandle(handle);
			}
		}

		api.DestroyDeviceInfoList(deviceInfo);
		return paired;
	}

	bool IsPaired(uint32_t joystick)
	{
		return joystick < k_MaxJoysticks && s_Devices[joystick].Handle != INVALID_HANDLE_VALUE;
	}

	void Unpair(uint32_t joystick)
	{
		if (!IsPaired(joystick))
			return;
		CloseHandle(s_Devices[joystick].Handle);
		s_Devices[joystick] = Device{};
	}

	bool SetVibration(uint32_t joystick, float lowFrequency, float highFrequency)
	{
		if (!IsPaired(joystick))
			return false;

		Device& device = s_Devices[joystick];
		const uint8_t low = ToMotorByte(lowFrequency);
		const uint8_t high = ToMotorByte(highFrequency);
		// Each write waits out the pad's USB interval, so a repeat of the last one isn't sent.
		if (device.LastLow == low && device.LastHigh == high)
			return true;

		std::vector<uint8_t> report(device.ReportSize, 0);

		if (device.Kind == PadKind::DualShock4)
		{
			report[0] = 0x05;
			report[1] = 0x01; // rumble only, so the light bar keeps its colour
			report[4] = high;
			report[5] = low;
		}
		else
		{
			// Report 0x02's fields start at byte 1: two flag bytes, then the right (light) and left
			// (heavy) motors, with a third flag byte at 39.
			report[0] = 0x02;
			report[1] = 0x02; // the motors, not the speaker-driven haptics
			if (device.ImprovedRumble)
			{
				report[39] = 0x04;
				report[3] = high;
				report[4] = low;
			}
			else
			{
				// The older emulation runs at about twice an Xbox pad's strength.
				report[1] |= 0x01;
				report[3] = static_cast<uint8_t>(high >> 1);
				report[4] = static_cast<uint8_t>(low >> 1);
			}
		}

		const bool written = WriteReport(device, report);
		device.LastLow = written ? low : -1;
		device.LastHigh = written ? high : -1;
		return written;
	}

#elif defined(DE_PLATFORM_LINUX)

	namespace
	{
		struct Device
		{
			int Fd = -1;
			std::string Path;
			int16_t EffectId = -1;
		};

		std::array<Device, k_MaxJoysticks> s_Devices{};

		constexpr size_t k_BitsPerLong = sizeof(unsigned long) * CHAR_BIT;

		bool HasRumble(int fd)
		{
			unsigned long bits[(FF_MAX + k_BitsPerLong) / k_BitsPerLong]{};
			if (ioctl(fd, EVIOCGBIT(EV_FF, sizeof(bits)), bits) < 0)
				return false;
			return (bits[FF_RUMBLE / k_BitsPerLong] >> (FF_RUMBLE % k_BitsPerLong)) & 1ul;
		}

		bool HeldByAnother(const std::string& path)
		{
			return std::any_of(s_Devices.begin(), s_Devices.end(), [&](const Device& device) { return device.Fd >= 0 && device.Path == path; });
		}

		// Sorted by name as GLFW sorts them at start-up, so identical pads pair with the nodes GLFW
		// gave their joystick ids; one plugged in later finds its node the only one left.
		std::vector<std::string> EventNodes()
		{
			std::vector<std::string> paths;
			if (DIR* dir = opendir("/dev/input"))
			{
				while (dirent* entry = readdir(dir))
				{
					if (std::strncmp(entry->d_name, "event", 5) == 0)
						paths.push_back(std::string("/dev/input/") + entry->d_name);
				}
				closedir(dir);
			}
			std::sort(paths.begin(), paths.end());
			return paths;
		}
	}

	bool Pair(uint32_t joystick, const DeviceIds& ids, const char* name)
	{
		if (joystick >= k_MaxJoysticks)
			return false;
		if (s_Devices[joystick].Fd >= 0)
			return true;
		const bool haveIds = ids.Vendor != 0 && ids.Product != 0;
		if (!haveIds && (!name || !*name))
			return false;

		for (const std::string& path : EventNodes())
		{
			if (HeldByAnother(path))
				continue;

			// Write access is what playing an effect needs; GLFW's own read-only handle can't.
			const int fd = open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
			if (fd < 0)
				continue;

			input_id id{};
			char deviceName[256] = "";
			const bool sameIds = !haveIds || (ioctl(fd, EVIOCGID, &id) >= 0 && id.bustype == ids.Bus && id.vendor == ids.Vendor
				&& id.product == ids.Product && id.version == ids.Version);
			const bool sameName = !name || (ioctl(fd, EVIOCGNAME(sizeof(deviceName)), deviceName) >= 0 && std::strncmp(deviceName, name, sizeof(deviceName)) == 0);
			if (sameIds && sameName && HasRumble(fd))
			{
				Device& device = s_Devices[joystick];
				device.Fd = fd;
				device.Path = path;
				device.EffectId = -1;
				return true;
			}
			close(fd);
		}
		return false;
	}

	bool IsPaired(uint32_t joystick)
	{
		return joystick < k_MaxJoysticks && s_Devices[joystick].Fd >= 0;
	}

	void Unpair(uint32_t joystick)
	{
		if (!IsPaired(joystick))
			return;
		Device& device = s_Devices[joystick];
		if (device.EffectId >= 0)
			ioctl(device.Fd, EVIOCRMFF, static_cast<int>(device.EffectId));
		close(device.Fd);
		device = Device{};
	}

	bool SetVibration(uint32_t joystick, float lowFrequency, float highFrequency)
	{
		if (!IsPaired(joystick))
			return false;

		Device& device = s_Devices[joystick];
		const uint8_t low = ToMotorByte(lowFrequency);
		const uint8_t high = ToMotorByte(highFrequency);

		input_event play{};
		play.type = EV_FF;

		if (low == 0 && high == 0)
		{
			if (device.EffectId < 0)
				return true;
			play.code = static_cast<uint16_t>(device.EffectId);
			play.value = 0;
			return write(device.Fd, &play, sizeof(play)) == static_cast<ssize_t>(sizeof(play));
		}

		// A replay length of 0 plays until stopped; Input times the rumble itself.
		ff_effect effect{};
		effect.type = FF_RUMBLE;
		effect.id = device.EffectId;
		effect.u.rumble.strong_magnitude = static_cast<uint16_t>(low * 257);
		effect.u.rumble.weak_magnitude = static_cast<uint16_t>(high * 257);
		if (ioctl(device.Fd, EVIOCSFF, &effect) < 0)
			return false;
		device.EffectId = effect.id;

		play.code = static_cast<uint16_t>(device.EffectId);
		play.value = 1;
		return write(device.Fd, &play, sizeof(play)) == static_cast<ssize_t>(sizeof(play));
	}

#else

	bool Pair(uint32_t, const DeviceIds&, const char*)
	{
		return false;
	}

	bool IsPaired(uint32_t)
	{
		return false;
	}

	void Unpair(uint32_t)
	{
	}

	bool SetVibration(uint32_t, float, float)
	{
		return false;
	}

#endif

}
