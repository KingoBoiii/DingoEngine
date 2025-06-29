#pragma once

#include <nvrhi/nvrhi.h>

namespace DingoEngine
{

	class VertexBuffer
	{
	public:
		static VertexBuffer* Create(const void* data, uint64_t size);

	public:
		VertexBuffer(const void* data, uint64_t size);

	public:
		void Initialize();
		void Destroy();

	private:
		nvrhi::BufferHandle m_BufferHandle;
		const void* m_Data = nullptr;
		uint64_t m_Size = 0;

		friend class CommandList;
	};

}
