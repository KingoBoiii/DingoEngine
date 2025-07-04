#pragma once
#include "DingoEngine/Graphics/GraphicsBuffer.h"

namespace DingoEngine
{

	class GraphicsBufferBuilder
	{
	public:
		GraphicsBufferBuilder() = default;
		~GraphicsBufferBuilder() = default;

	public:
		GraphicsBufferBuilder& SetDebugName(const std::string& debugName)
		{
			m_Params.DebugName = debugName;
			return *this;
		}

		GraphicsBufferBuilder& SetByteSize(uint64_t byteSize)
		{
			m_Params.ByteSize = byteSize;
			return *this;
		}

		GraphicsBufferBuilder& SetIsVolatile(bool isVolatile)
		{
			m_Params.IsVolatile = isVolatile;
			return *this;
		}

		GraphicsBufferBuilder& SetDirectUpload(bool directUpload)
		{
			m_Params.DirectUpload = directUpload;
			return *this;
		}

		GraphicsBufferBuilder& SetType(BufferType type)
		{
			m_Params.Type = type;
			return *this;
		}

		GraphicsBufferBuilder& SetFormat(GraphicsFormat format)
		{
			m_Params.Format = format;
			return *this;
		}

		GraphicsBufferBuilder& SetInitialData(const void* initialData)
		{
			m_InitialData = initialData;
			return *this;
		}

		GraphicsBuffer* Create()
		{
			GraphicsBuffer* buffer = GraphicsBuffer::Create(m_Params);
			buffer->Initialize();

			if (m_InitialData)
			{
				buffer->Upload(m_InitialData, m_Params.ByteSize);
			}

			return buffer;
		}

	private:
		GraphicsBufferParams m_Params;
		const void* m_InitialData = nullptr; 
	};

}

