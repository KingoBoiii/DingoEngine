#pragma once

#include <nvrhi/nvrhi.h>

namespace DingoEngine
{

	class VertexBuffer
	{
	public:
		static VertexBuffer* Create(const void* data, uint64_t size);

	public:
		VertexBuffer(const void* data, uint64_t size, bool usingBuilderPattern = false);

	public:
		void Initialize();
		void Destroy();

		void Upload(const void* data, uint64_t size, uint64_t offset = 0ul) const;

	private:
		nvrhi::BufferHandle m_BufferHandle;
		const void* m_Data = nullptr;
		uint64_t m_Size = 0;
		bool m_UsingBuilderPattern = false;

		friend class CommandList;
	};

	class IndexBuffer
	{
	public:
		static IndexBuffer* Create(const uint16_t* indices, uint32_t count);

	public:
		IndexBuffer(const uint16_t* indices, uint32_t count);

	public:
		void Initialize();
		void Destroy();

	private:
		nvrhi::BufferHandle m_BufferHandle;
		const uint16_t* m_Indices = nullptr;
		uint32_t m_Count = 0;

		friend class CommandList;
	};

}
