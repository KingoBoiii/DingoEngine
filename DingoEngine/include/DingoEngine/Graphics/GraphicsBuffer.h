#pragma once
#include "DingoEngine/Graphics/Enums/BufferType.h"

#include <nvrhi/nvrhi.h>

namespace DingoEngine
{

	enum class GraphicsFormat
	{
		Unknown,

		Uint16,
		Uint32,

		Count
	};

	struct GraphicsBufferParams
	{
		std::string DebugName;
		uint64_t ByteSize = 0;
		bool IsVolatile = false;
		bool DirectUpload = false;
		BufferType Type = BufferType::Unknown;
		GraphicsFormat Format = GraphicsFormat::Unknown;
		bool KeepInitialState = true;

		GraphicsBufferParams& SetDebugName(const std::string& debugName)
		{
			DebugName = debugName;
			return *this;
		}

		GraphicsBufferParams& SetByteSize(uint64_t byteSize)
		{
			ByteSize = byteSize;
			return *this;
		}

		GraphicsBufferParams& SetIsVolatile(bool isVolatile)
		{
			IsVolatile = isVolatile;
			return *this;
		}

		GraphicsBufferParams& SetDirectUpload(bool directUpload)
		{
			DirectUpload = directUpload;
			return *this;
		}

		GraphicsBufferParams& SetType(BufferType type)
		{
			Type = type;
			return *this;
		}

		GraphicsBufferParams& SetFormat(GraphicsFormat format)
		{
			Format = format;
			return *this;
		}
	};

	template<typename T>
	class GenericGraphicsBuffer
	{
	public:
		GenericGraphicsBuffer() = delete;
		virtual ~GenericGraphicsBuffer() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Destroy() = 0;
		virtual void Upload(const void* data, uint64_t size, uint64_t offset = 0ul) = 0;

		virtual const BufferType GetType() const { return m_Params.Type; }
		virtual const GraphicsFormat GetFormat() const { return m_Params.Format; }
		virtual const uint64_t GetByteSize() const { return m_Params.ByteSize; }

		virtual const bool IsType(BufferType type) const { return m_Params.Type == type; }

	protected:
		GenericGraphicsBuffer(const GraphicsBufferParams& params)
			: m_Params(params)
		{}

	protected:
		GraphicsBufferParams m_Params;
		nvrhi::BufferHandle m_BufferHandle;
		T* m_Data = nullptr;

		friend class CommandList;
		friend class Pipeline;
	};

	class GraphicsBuffer : public GenericGraphicsBuffer<const void>
	{
	public:
		static GraphicsBuffer* Create(const GraphicsBufferParams& params);

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;
		virtual void Upload(const void* data, uint64_t size, uint64_t offset = 0ul) override;

	private:
		GraphicsBuffer(const GraphicsBufferParams& params)
			: GenericGraphicsBuffer<const void>(params)
		{}
	};

	class IndexBufferUint16 : public GenericGraphicsBuffer<const uint16_t>
	{
	};

	class IndexBufferUint32 : public GenericGraphicsBuffer<const uint32_t>
	{
	};

}
