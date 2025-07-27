#include "depch.h"
#include "DingoEngine/Core/Memory.h"

#ifdef ENABLE_MEMORY_TRACKING

namespace Dingo
{

	const AllocationStats& Memory::GetAllocationStats()
	{
		return AllocationStats();
	}

}

static void* Alloc(size_t size)
{
	return malloc(size);
}

static void Free(void* data)
{
	return free(data);
}

void* operator new(size_t size)
{
	return Alloc(size);
}

void* operator new[](size_t size)
{
	return Alloc(size);
}

void operator delete(void* memory)
{
	Free(memory);
}

void operator delete[](void* memory)
{
	Free(memory);
}

#endif // ENABLE_MEMORY_TRACKING


