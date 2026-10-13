#pragma once

namespace Dingo
{

	/// How a fragment's colour combines with the target's (PipelineParams::BlendMode).
	enum class BlendMode
	{
		/// Straight-alpha "over": colour SrcAlpha/OneMinusSrcAlpha, alpha One/OneMinusSrcAlpha.
		Alpha,
		/// Adds the source to the target, colour and alpha.
		Additive,
		/// Writes the source as it is.
		Opaque,
		/// Multiplies the target's colour by the source's, keeping the target's alpha.
		Multiply
	};

}
