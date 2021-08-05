#include "cwpch.h"

#include "Crowny/Memory/Allocator.h"
#include "Crowny/Memory/MemoryManager.h"

namespace Crowny
{
	// alligned malloc?
	void* Allocator::Allocate(size_t size) noexcept
	{
		MemoryManager::Get().m_MemoryStats.TotalAllocated += size;
		MemoryManager::Get().m_MemoryStats.CurrentAllcoated += size;
		MemoryManager::Get().m_MemoryStats.Allocations++;
		return malloc(size);
	}

	void Allocator::Free(void* block) noexcept
	{
		size_t size = sizeof(block);
		MemoryManager::Get().m_MemoryStats.TotalFreed += size;
		MemoryManager::Get().m_MemoryStats.CurrentAllcoated -= size;
		free(block);
	}
}