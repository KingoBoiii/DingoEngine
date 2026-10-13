#pragma once

namespace Dingo
{

	/// Base of the resources a shader can bind: textures, samplers and buffers.
	class IBindableShaderResource
	{
	public:
		/// Frees the GPU resource.
		virtual ~IBindableShaderResource() = default;

	public:
		/// The backend's native resource object, for engine internals; game code has no use for it.
		virtual const void* GetResourceHandle() const = 0;
	};

}
