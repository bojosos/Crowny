#pragma once
#include "Buffer.h"

namespace Crowny
{

	enum class BufferUsage
	{
		STATIC_DRAW,
		DYNAMIC_DRAW
	};

	struct VertexBufferProperties
	{
		BufferUsage Usage = BufferUsage::STATIC_DRAW;
	};

	class VertexBuffer
	{
	public:
		virtual ~VertexBuffer() = default;
		
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void SetData(const void* data, uint32_t size) = 0;

		virtual const BufferLayout& GetLayout() const = 0;
		virtual void SetLayout(const BufferLayout& layout) = 0;
		virtual void* GetPointer(uint32_t size) const = 0;
		virtual void FreePointer() const = 0;

		static Ref<VertexBuffer> Create(float* vertices, uint32_t size, const VertexBufferProperties& props = {});
	};
}