#pragma once
#include <nvrhi/nvrhi.h>

namespace DingoEngine
{

	class UniformBuffer
	{
	public:
		static UniformBuffer* Create(uint64_t size);

	public:
		UniformBuffer(uint64_t size);
		~UniformBuffer() = default;

	public:
		void Initialize();
		void Destroy();

		void Upload(const void* data);

	private:
		nvrhi::BufferHandle m_BufferHandle = nullptr;
		const void* m_Data = nullptr; // Pointer to the data, if needed
		uint64_t m_Size;

		friend class CommandList; 
		friend class Pipeline; 
	};

}
