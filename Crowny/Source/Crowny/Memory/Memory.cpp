#include "cwpch.h"

#include "Crowny/Memory/Memory.h"

#if CW_TRACK_MEMORY

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size)
{
	return Crowny::Allocator::Allocate(size);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size)
{
	return Crowny::Allocator::Allocate(size);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, const char* desc)
{
	return Crowny::Allocator::Allocate(size, desc);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, const char* desc)
{
	return Crowny::Allocator::Allocate(size, desc);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, const char* file, int line)
{
	return Crowny::Allocator::Allocate(size, file, line);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, const char* file, int line)
{
	return Crowny::Allocator::Allocate(size, file, line);
}

void __CRTDECL operator delete(void* memory)
{
	return Crowny::Allocator::Free(memory);
}

void __CRTDECL operator delete(void* memory, const char* desc)
{
	return Crowny::Allocator::Free(memory);
}

void __CRTDECL operator delete(void* memory, const char* file, int line)
{
	return Crowny::Allocator::Free(memory);
}

void __CRTDECL operator delete[](void* memory)
{
	return Crowny::Allocator::Free(memory);
}

void __CRTDECL operator delete[](void* memory, const char* desc)
{
	return Crowny::Allocator::Free(memory);
}

void __CRTDECL operator delete[](void* memory, const char* file, int line)
{
	return Crowny::Allocator::Free(memory);
}

#endif
