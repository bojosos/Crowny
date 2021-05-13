#pragma once

#include "Crowny/Renderer/Buffer.h"

namespace Crowny
{

	enum class BufferUsage
	{
		STATIC_DRAW,
		DYNAMIC_DRAW
	};
	
	enum class IndexType
	{
		Index16,
		Index32
	};
	
	enum class GpuLockOptions
	{
		READ_ONLY,
		WRITE_ONLY,
		WRITE_DISCARD
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

		virtual void SetData(void* data, uint32_t size) = 0;

		virtual const BufferLayout& GetLayout() const = 0;
		virtual void SetLayout(const BufferLayout& layout) = 0;

		virtual void* GetPointer(uint32_t size) = 0;
		virtual void FreePointer() = 0;

		static Ref<VertexBuffer> Create(void* vertices, uint32_t size, const VertexBufferProperties& props = {});
	};
}