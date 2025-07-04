#pragma once
#include "DingoEngine/Graphics/Buffer.h"

namespace DingoEngine
{

	// Usage:
	// IndexBuffer* indexBuffer = IndexBufferBuilder()
	//	.SetCount(uint32_t)
	//	.SetIndices(const void*)
	//	.Create();
	class IndexBufferBuilder
	{
	public:
		IndexBufferBuilder() = default;
		~IndexBufferBuilder() = default;

		IndexBufferBuilder& SetCount(uint32_t count)
		{
			m_Count = count;
			m_Size = count * sizeof(uint16_t);
			return *this;
		}

		IndexBufferBuilder& SetIndices(const uint16_t* indices)
		{
			m_Indices = indices;
			return *this;
		}

		IndexBuffer* Create()
		{
			auto buffer = new IndexBuffer(m_Indices, m_Count, true);
			buffer->Initialize();

			if (m_Indices)
			{
				buffer->Upload(m_Indices, m_Size);
			}

			return buffer;
		}

	private:
		uint32_t m_Count = 0;
		uint64_t m_Size = 0;
		const uint16_t* m_Indices = nullptr;
	};

}
