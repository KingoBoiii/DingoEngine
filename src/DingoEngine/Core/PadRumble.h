#pragma once

#include <cstdint>

// Engine-internal: rumble for pads XInput doesn't reach. On Windows it writes HID output reports to
// Sony pads (DualShock 4, DualSense, DualSense Edge) connected over USB; on Linux it plays an evdev
// FF_RUMBLE effect on any pad whose event node has one. Elsewhere nothing pairs.

namespace Dingo::Internal::PadRumble
{

	inline constexpr uint32_t k_MaxJoysticks = 16;

	struct DeviceIds
	{
		uint16_t Bus = 0;
		uint16_t Vendor = 0;
		uint16_t Product = 0;
		uint16_t Version = 0;
	};

	// GLFW builds a joystick's GUID as SDL does: bus, vendor, product and version as little-endian
	// 16-bit fields at bytes 0, 4, 8 and 12, each followed by two zero bytes. A GUID made from the
	// name, or an XInput one, reads as all zero.
	DeviceIds ParseGlfwGuid(const char* guid);

	// Pairs the joystick with a rumble device of the same ids that no other joystick holds. True if
	// it is now paired. `name` is GLFW's joystick name, compared with the device's on Linux, where one
	// pad has several event nodes with the same ids, and the only key there when the ids are zero.
	bool Pair(uint32_t joystick, const DeviceIds& ids, const char* name);
	bool IsPaired(uint32_t joystick);
	void Unpair(uint32_t joystick);

	// Motor speeds from 0 to 1: lowFrequency drives the heavy left motor, highFrequency the light right one.
	bool SetVibration(uint32_t joystick, float lowFrequency, float highFrequency);

}
