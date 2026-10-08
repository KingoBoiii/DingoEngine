#pragma once
#include "DingoEngine/Graphics/Enums/BufferType.h"
#include "DingoEngine/Graphics/Enums/GraphicsFormat.h"
#include "DingoEngine/Graphics/IBindableShaderResource.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Dingo
{

	struct GraphicsBufferParams
	{
		std::string DebugName;
		uint64_t ByteSize = 0;
		bool IsVolatile = false;
		uint32_t MaxWritesPerFrame = 8; // Volatile buffers: uploads per frame before Vulkan drops one
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

		GraphicsBufferParams& SetMaxWritesPerFrame(uint32_t maxWritesPerFrame)
		{
			MaxWritesPerFrame = maxWritesPerFrame;
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

	class GraphicsBuffer : public GenericGraphicsBuffer<const void>, public IBindableShaderResource
	{
	public:
		static GraphicsBuffer* CreateVertexBuffer(uint64_t size, const void* data = nullptr, bool directUpload = true, const std::string& debugName = "Vertex Buffer");
		static GraphicsBuffer* CreateIndexBuffer(uint64_t size, const void* data = nullptr, bool directUpload = true, const std::string& debugName = "Index Buffer", GraphicsFormat indexFormat = GraphicsFormat::Uint16);
		static GraphicsBuffer* CreateUniformBuffer(uint64_t size, const std::string& debugName = "Uniform Buffer");
		// A GPU-writable buffer: a shader's storage block (std430). Compute writes it; any stage reads
		// it, a vertex stage only when its block is readonly. Upload writes it from the CPU.
		static GraphicsBuffer* CreateStorageBuffer(uint64_t size, const std::string& debugName = "Storage Buffer");
		static GraphicsBuffer* Create(const GraphicsBufferParams& params);

		// Never reused, unlike the buffer's address, so a cache keyed on it cannot hand a freed
		// buffer's bindings to a new buffer allocated at the same address.
		uint64_t GetId() const { return m_Id; }

		// Copies size bytes from offset (all of it from offset when size is 0) back to the CPU, with
		// Texture::ReadPixels' timing: inside a frame the copy follows the frame's earlier work and
		// done runs at the next frame's start; between frames it waits for the next frame. done gets
		// an empty vector when the buffer can't be read. Not for volatile buffers.
		virtual void ReadBack(std::function<void(const std::vector<uint8_t>&)> done, uint64_t offset = 0, uint64_t size = 0) = 0;

	protected:
		GraphicsBuffer(const GraphicsBufferParams& params)
			: GenericGraphicsBuffer<const void>(params)
		{}
		virtual ~GraphicsBuffer() = default;

	private:
		static uint64_t AllocateId();

		uint64_t m_Id = AllocateId();
	};

}
