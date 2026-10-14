#pragma once
#include "DingoEngine/Graphics/Enums/BufferType.h"
#include "DingoEngine/Graphics/Enums/BufferUpload.h"
#include "DingoEngine/Graphics/Enums/IndexFormat.h"
#include "DingoEngine/Graphics/IBindableShaderResource.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Dingo
{

	/// Describes a GraphicsBuffer for GraphicsBuffer::Create.
	struct GraphicsBufferParams
	{
		std::string DebugName; ///< Name shown in graphics debuggers and logs.
		uint64_t ByteSize = 0; ///< Size in bytes; required, must be greater than 0.
		/// Makes a volatile buffer: each write gets its own GPU-side version, for per-frame data
		/// such as uniforms. It must be written in every frame that binds it, or Vulkan reads stale
		/// data and D3D12 removes the device. Not for storage buffers, and ReadBack refuses it.
		bool IsVolatile = false;
		/// Volatile buffers: uploads per frame before Vulkan drops one
		uint32_t MaxWritesPerFrame = 8;
		BufferUpload Upload = BufferUpload::Deferred; ///< When Upload runs; see BufferUpload.
		BufferType Type = BufferType::Unknown; ///< What the buffer is bound as; required.
		Dingo::IndexFormat IndexFormat = Dingo::IndexFormat::Uint32; ///< index buffers only
		/// Keeps the buffer in its type's initial resource state between uses (NVRHI
		/// keepInitialState).
		bool KeepInitialState = true;

		/// Pointer to initial data, used for direct upload buffers
		const void* InitialData = nullptr;

		/// Each setter sets the field of the same name and returns *this, for chaining.
		///@{
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

		GraphicsBufferParams& SetUpload(BufferUpload upload)
		{
			Upload = upload;
			return *this;
		}

		GraphicsBufferParams& SetType(BufferType type)
		{
			Type = type;
			return *this;
		}

		GraphicsBufferParams& SetIndexFormat(Dingo::IndexFormat format)
		{
			IndexFormat = format;
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
		///@}
	};

	/// Common buffer interface over the pointer type the CPU data is held as; use GraphicsBuffer.
	template<typename T>
	class GenericGraphicsBuffer
	{
	public:
		/// Not default-constructible; buffers are made through GraphicsBuffer's factories.
		GenericGraphicsBuffer() = delete;
		/// Frees the GPU buffer.
		virtual ~GenericGraphicsBuffer() = default;

	protected:
		/// Create() calls it, so a resource is ready when the factory returns it.
		virtual void Initialize() = 0;

	public:
		/// Releases the GPU object early; the destructor releases everything anyway.
		virtual void Destroy() = 0;
		/// Writes size bytes at byte offset, which must fit in ByteSize (and be 0 for a volatile
		/// buffer, as Renderer::Upload). A Direct buffer uploads at
		/// once; a Deferred one only keeps the pointer, size and offset, and Renderer::Upload(buffer)
		/// sends that range (the pointer must outlive that frame). A storage buffer copies the bytes
		/// at once or at the next frame's start (see ReadBack's timing).
		virtual void Upload(T* data, uint64_t size, uint64_t offset = 0ul) = 0;

		/// What the buffer is bound as.
		virtual const BufferType GetType() const { return m_Params.Type; }
		/// Index width of an index buffer; meaningless for other types.
		virtual const IndexFormat GetIndexFormat() const { return m_Params.IndexFormat; }
		/// Size in bytes, as created.
		virtual const uint64_t GetByteSize() const { return m_Params.ByteSize; }
		/// Index count of an index buffer, ByteSize over the size of its IndexFormat; 0 for other
		/// types.
		virtual const uint32_t GetIndexCount() const { return 0; }
		/// The pointer the last Upload was given, not a copy; nullptr when none, and always for
		/// storage buffers.
		virtual T* GetData() const { return m_Data; }

		/// True when the buffer's type is the given one.
		virtual const bool IsType(BufferType type) const { return m_Params.Type == type; }

	protected:
		/// For subclasses: takes the params the buffer was created with.
		GenericGraphicsBuffer(const GraphicsBufferParams& params)
			: m_Params(params)
		{}

	protected:
		GraphicsBufferParams m_Params; ///< The params the buffer was created with.
		T* m_Data = nullptr; ///< The pointer the last Upload was given.
	};

	/// A GPU buffer: vertex, index, uniform or storage. The caller owns it; delete frees the GPU
	/// object.
	class GraphicsBuffer : public GenericGraphicsBuffer<const void>, public IBindableShaderResource
	{
	public:
		/// A vertex buffer of size bytes; data, if given, is uploaded as upload says (Direct: now).
		[[nodiscard]] static GraphicsBuffer* CreateVertexBuffer(uint64_t size, const void* data = nullptr, BufferUpload upload = BufferUpload::Direct, const std::string& debugName = "Vertex Buffer");
		/// An index buffer of size bytes holding indices of format; data is uploaded as for
		/// CreateVertexBuffer.
		[[nodiscard]] static GraphicsBuffer* CreateIndexBuffer(uint64_t size, IndexFormat format, const void* data = nullptr, BufferUpload upload = BufferUpload::Direct, const std::string& debugName = "Index Buffer");
		/// A volatile uniform buffer of size bytes: write it with Renderer::Upload in every frame
		/// that binds it, or Vulkan reads stale data and D3D12 removes the device.
		[[nodiscard]] static GraphicsBuffer* CreateUniformBuffer(uint64_t size, const std::string& debugName = "Uniform Buffer");
		/// A GPU-writable buffer: a shader's storage block (std430). Compute writes it; any stage
		/// reads it, a vertex stage only when its block is readonly. Upload (and initialData, size
		/// bytes) writes it from the CPU with ReadBack's timing: into the frame's command list
		/// inside a frame, at once before the first frame, else at the next frame's start. The
		/// bytes are copied, never kept.
		[[nodiscard]] static GraphicsBuffer* CreateStorageBuffer(uint64_t size, const std::string& debugName = "Storage Buffer", const void* initialData = nullptr);
		/// Creates a buffer from params, ready to use; uploads InitialData when set.
		[[nodiscard]] static GraphicsBuffer* Create(const GraphicsBufferParams& params);

		/// Never reused, unlike the buffer's address, so a cache keyed on it cannot hand a freed
		/// buffer's bindings to a new buffer allocated at the same address.
		uint64_t GetId() const { return m_Id; }

		/// Copies size bytes from offset (all of it from offset when size is 0) back to the CPU,
		/// with Texture::ReadPixels' timing: inside a frame the copy follows the frame's earlier
		/// work and done runs at the next frame's start; between frames it waits for the next
		/// frame. done gets an empty vector when the buffer can't be read. Not for volatile
		/// buffers.
		virtual void ReadBack(std::function<void(const std::vector<uint8_t>&)> done, uint64_t offset = 0, uint64_t size = 0) = 0;

		/// Frees the GPU buffer; never delete one a frame still in flight draws from.
		virtual ~GraphicsBuffer() = default;

	protected:
		/// For subclasses: takes the params the buffer was created with.
		GraphicsBuffer(const GraphicsBufferParams& params)
			: GenericGraphicsBuffer<const void>(params)
		{}

	private:
		static uint64_t AllocateId();

		uint64_t m_Id = AllocateId();
	};

}
