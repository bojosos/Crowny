#pragma once

#include "Crowny/Memory/Allocator.h"

#define CW_TRACK_MEMORY 1
#if CW_TRACK_MEMORY

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, const char* desc);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, const char* desc);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, const char* file, int line);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, const char* file, int line);

void __CRTDECL operator delete(void* memory);
void __CRTDECL operator delete(void* memory, const char* desc);
void __CRTDECL operator delete(void* memory, const char* file, int line);
void __CRTDECL operator delete[](void* memory);
void __CRTDECL operator delete[](void* memory, const char* desc);
void __CRTDECL operator delete[](void* memory, const char* file, int line);

#define cwnew new(__FILE__, __LINE__)
#define delete delete

#else

#define cwnew new
#define delete delete

#endif


//void* operator new(size_t size) { return Crowny::Allocator::Allocate(size); }
//
//void* operator new[](size_t size) { return Crowny::Allocator::Allocate(size); }
//
//void operator delete(void* block) { Crowny::Allocator::Free(block); }
//
//void operator delete[](void* block) { Crowny::Allocator::Free(block); }