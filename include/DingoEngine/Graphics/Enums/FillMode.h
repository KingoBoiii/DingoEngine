#pragma once

namespace Dingo
{

	/// How triangles are rasterised (PipelineParams::FillMode).
	enum class FillMode
	{
		Solid,    ///< Filled triangles.
		Wireframe ///< Triangle edges only.
	};

}
