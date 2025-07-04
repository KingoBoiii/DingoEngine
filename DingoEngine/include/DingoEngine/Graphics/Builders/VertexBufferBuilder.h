#pragma once
#include "DingoEngine/Graphics/Buffer.h"

namespace DingoEngine
{

	// Usage:
	// VertexBuffer* vertexBuffer = VertexBufferBuilder()
	//	.SetSize(uint64_t)
	//	.SetData(const void*)
	//	.Create();
	class VertexBufferBuilder
	{
	public:
		VertexBufferBuilder() = default;
		~VertexBufferBuilder() = default;

		VertexBufferBuilder& SetSize(uint64_t size)
		{
			m_Size = size;
			return *this;
		}

		VertexBufferBuilder& SetData(const void* data)
		{
			m_Data = data;
			return *this;
		}

		VertexBuffer* Create()
		{
			auto buffer = new VertexBuffer(m_Data, m_Size, true);
			buffer->Initialize();

			if (m_Data)
			{
				buffer->Upload(m_Data, m_Size);
			}

			return buffer;
		}

	private:
		uint64_t m_Size = 0;
		const void* m_Data = nullptr;
	};

}
