#pragma once

namespace Crowny
{
	class Allocator
	{
	public:
		static void* Allocate(size_t size) noexcept;

		static void Free(void* block) noexcept;
	};
}