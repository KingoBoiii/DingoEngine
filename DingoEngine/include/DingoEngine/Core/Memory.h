#pragma once

namespace DingoEngine
{

	struct AllocationStats
	{
		size_t TotalAllocated = 0;
		size_t TotalFreed = 0;
	};

	class Memory
	{
		const AllocationStats& GetAllocationStats();
	};

}

#ifdef DE_PLATFORM_WINDOWS

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size);

void __CRTDECL operator delete(void* memory);
void __CRTDECL operator delete[](void* memory);

#else
#endif

