#pragma once

namespace Dingo
{

	enum class SamplerAddressMode
	{
		Clamp,
		Wrap,
		Border,
		Mirror,
		MirrorOnce,

		ClampToEdge = Clamp,
		Repeat = Wrap,
		ClampToBorder = Border,
		MirroredRepeat = Mirror,
		MirrorClampToEdge = MirrorOnce
	};

}
