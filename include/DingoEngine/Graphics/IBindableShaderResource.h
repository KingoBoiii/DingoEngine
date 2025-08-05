#pragma once

namespace Dingo
{

	class IBindableShaderResource
	{
	public:
		virtual ~IBindableShaderResource() = default;

	public:
		virtual const void* GetResourceHandle() const = 0;
	};

}
