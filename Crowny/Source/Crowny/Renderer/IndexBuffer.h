#pragma once
#include "Buffer.h"

namespace Crowny
{

	class IndexBuffer
	{
	public:
		virtual ~IndexBuffer() = default;
		
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual uint32_t GetCount() const = 0;

		static Ref<IndexBuffer> Create(uint32_t size);
		static Ref<IndexBuffer> Create(uint32_t* indices, uint32_t size);
	};
}