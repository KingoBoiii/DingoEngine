#pragma once
#include "DingoEngine/Graphics/Enums/BufferType.h"
#include "DingoEngine/Graphics/Enums/GraphicsFormat.h"
#include "DingoEngine/Graphics/IBindableShaderResource.h"

namespace Dingo
{

	struct GraphicsBufferParams
	{
		std::string DebugName;
		uint64_t ByteSize = 0;
		bool IsVolatile = false;
		bool DirectUpload = false;
		BufferType Type = BufferType::Unknown;
		GraphicsFormat Format = GraphicsFormat::Unknown;
		bool KeepInitialState = true;

		const void* InitialData = nullptr; // Pointer to initial data, used for direct upload buffers

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

		GraphicsBufferParams& SetKeepInitialState(bool keepInitialState)
		{
			KeepInitialState = keepInitialState;
			return *this;
		}

		GraphicsBufferParams& SetInitialData(const void* initialData)
		{
			InitialData = initialData;
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
		virtual void Upload(T* data, uint64_t size, uint64_t offset = 0ul) = 0;

		virtual const BufferType GetType() const { return m_Params.Type; }
		virtual const GraphicsFormat GetFormat() const { return m_Params.Format; }
		virtual const uint64_t GetByteSize() const { return m_Params.ByteSize; }
		virtual const uint32_t GetIndexCount() const { return 0; }
		virtual T* GetData() const { return m_Data; }

		virtual const bool IsType(BufferType type) const { return m_Params.Type == type; }

	protected:
		GenericGraphicsBuffer(const GraphicsBufferParams& params)
			: m_Params(params)
		{}

	protected:
		GraphicsBufferParams m_Params;
		T* m_Data = nullptr;
	};

	/*
		m_UniformBuffer = Dingo::GraphicsBufferBuilder()
			.SetDebugName("Uniform Buffer")
			.SetByteSize(sizeof(CameraTransform))
			.SetType(Dingo::BufferType::UniformBuffer)
			.SetIsVolatile(true)
			.SetDirectUpload(false)
			.Create();
	*/

	class GraphicsBuffer : public GenericGraphicsBuffer<const void>, public IBindableShaderResource
	{
	public:
		static GraphicsBuffer* CreateVertexBuffer(uint64_t size, const void* data = nullptr, bool directUpload = true, const std::string& debugName = "Vertex Buffer");
		static GraphicsBuffer* CreateIndexBuffer(uint64_t size, const void* data = nullptr, bool directUpload = true, const std::string& debugName = "Index Buffer");
		static GraphicsBuffer* CreateUniformBuffer(uint64_t size, const void* data = nullptr, const std::string& debugName = "Uniform Buffer");
		static GraphicsBuffer* Create(const GraphicsBufferParams& params);

	protected:
		GraphicsBuffer(const GraphicsBufferParams& params)
			: GenericGraphicsBuffer<const void>(params)
		{}
		virtual ~GraphicsBuffer() = default;
	};

}
