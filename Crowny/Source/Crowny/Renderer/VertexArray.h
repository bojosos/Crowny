#pragma once

#include "Crowny/Renderer/Buffer.h"
#include "Crowny/Renderer/VertexBuffer.h"
#include "Crowny/Renderer/IndexBuffer.h"

namespace Crowny
{
	class VertexArray
	{
	public:
		virtual ~VertexArray() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void AddVertexBuffer(const Ref<VertexBuffer>& vbo) = 0;

		virtual void SetIndexBuffer(const Ref<IndexBuffer>& ibo) = 0;

		virtual const std::vector<Ref<VertexBuffer>>& GetVertexBuffers() const = 0;
		virtual const Ref<IndexBuffer>& GetIndexBuffer() const = 0;

		static Ref<VertexArray> Create();
	};
}