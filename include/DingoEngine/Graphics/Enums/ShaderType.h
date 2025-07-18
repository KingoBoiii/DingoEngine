#pragma once

namespace Dingo
{

	enum class ShaderType
	{
		Vertex,
		Fragment,
		Geometry,
		TessellationControl,
		TessellationEvaluation,
		Compute,
		RayGeneration,
		RayIntersection,
		RayAnyHit,
		RayClosestHit,
		RayMiss,
		RayCallable,

		Pixel = Fragment, // Alias for compatibility
	};

}
