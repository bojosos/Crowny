#include "cwpch.h"
#include "Memory.h"
#include "Crowny/Common/Common.h"
#include <memory>

/*
void* operator new(size_t size)
{
	return Minecraft::Memory::Allocate(size);
}

void* operator new[](size_t size)
{
	return Minecraft::Memory::Allocate(size);
}

void operator delete(void* memory, size_t size)
{
	Minecraft::Memory::Free(memory, size);
}

void operator delete[](void* memory, size_t size)
{
	Minecraft::Memory::Free(memory, size);
}
*/

namespace Crowny
{

	void* Memory::Allocate(size_t size)
	{
		Get().m_Allocated += size;
		return malloc(size);
	}

	void Memory::Free(void* memory, size_t size)
	{
		Get().m_Freed += size;
		free(memory);
	}

	size_t Memory::GetAllocated()
	{
		return Get().m_Allocated - Get().m_Freed;
	}

}