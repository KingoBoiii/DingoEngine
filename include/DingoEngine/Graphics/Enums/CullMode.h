#pragma once

namespace Dingo
{

	/// Which triangle faces a pipeline discards; the front face is set by
	/// PipelineParams::FrontCounterClockwise.
	enum class CullMode
	{
		None,  ///< Draws both faces.
		Front, ///< Discards front faces.
		Back   ///< Discards back faces.
	};

}
