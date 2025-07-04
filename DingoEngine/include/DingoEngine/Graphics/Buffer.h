#pragma once

#include <nvrhi/nvrhi.h>

namespace DingoEngine
{

	class VertexBuffer
	{
	public:
		static VertexBuffer* Create(const void* data, uint64_t size);

	public:
		VertexBuffer() = delete;
		~VertexBuffer() = default;

	public:
		void Initialize();
		void Destroy();

		void Upload(const void* data, uint64_t size, uint64_t offset = 0ul) const;

	private:
		VertexBuffer(const void* data, uint64_t size)
			: m_Data(data), m_Size(size)
		{}

	private:
		nvrhi::BufferHandle m_BufferHandle;
		const void* m_Data = nullptr;
		uint64_t m_Size = 0;

		friend class CommandList;
	};

	class IndexBuffer
	{
	public:
		static IndexBuffer* Create(const uint16_t* indices, uint32_t count);

	public:
		IndexBuffer() = delete;
		~IndexBuffer() = default;

	public:
		void Initialize();
		void Destroy();

		void Upload(const uint16_t* indices, uint64_t size, uint64_t offset = 0ul) const;

	private:
		IndexBuffer(const uint16_t* indices, uint32_t count)
			: m_Indices(indices), m_Count(count), m_Size(count * sizeof(uint16_t))
		{}

	private:
		nvrhi::BufferHandle m_BufferHandle;
		const uint16_t* m_Indices = nullptr;
		uint32_t m_Count = 0;
		uint64_t m_Size = 0;

		friend class CommandList;
	};

}
