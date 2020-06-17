#pragma once

namespace Crowny
{
	class Memory
	{
	public:
		static void* Allocate(size_t size);

		static void Free(void* memory, size_t size);

		static size_t GetAllocated();

	private:
		Memory() = default;
		static Memory& Get()
		{
			static Memory instance;
			return instance;
		}

		size_t m_Allocated = 0, m_Freed = 0;
	};
}

