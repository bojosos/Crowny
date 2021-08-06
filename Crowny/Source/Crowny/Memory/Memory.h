#pragma once

#include "Crowny/Memory/Allocator.h"

void* operator new(size_t size) { return Crowny::Allocator::Allocate(size); }

void* operator new[](size_t size) { return Crowny::Allocator::Allocate(size); }

void operator delete(void* block) { Crowny::Allocator::Free(block); }

void operator delete[](void* block) { Crowny::Allocator::Free(block); }