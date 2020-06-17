#include "cwpch.h"
#include "OpenGLVertexBuffer.h"

#ifdef MC_WEB
#include <GLES3/gl32.h>
#else
#include <glad/glad.h>
#endif

namespace Crowny
{

	static uint32_t BufferUsageToOpenGLBufferUsage(BufferUsage usage)
	{
		switch (usage)
		{
		case(BufferUsage::STATIC_DRAW): return GL_STATIC_DRAW;
		case(BufferUsage::DYNAMIC_DRAW): return GL_DYNAMIC_DRAW;
		}
		CW_ENGINE_ASSERT(false, "Drawing mode not support");
		return GL_NONE;
	}

	OpenGLVertexBuffer::OpenGLVertexBuffer(const void* data, uint32_t size, const VertexBufferProperties& properties) : m_Size(size), m_Properties(properties)
	{
#ifdef MC_WEB
		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ARRAY_BUFFER, size, data, BufferUsageToOpenGLBufferUsage(m_Properties.Usage));
#else
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, size, data, BufferUsageToOpenGLBufferUsage(m_Properties.Usage));
#endif	
	}

	OpenGLVertexBuffer::~OpenGLVertexBuffer()
	{
		glDeleteBuffers(1, &m_RendererID);
	}

	void OpenGLVertexBuffer::SetData(const void* verts, uint32_t size)
	{
#ifdef MC_WEB
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ARRAY_BUFFER, size, verts, BufferUsageToOpenGLBufferUsage(m_Properties.Usage));
#else
		glNamedBufferData(m_RendererID, size, verts, BufferUsageToOpenGLBufferUsage(m_Properties.Usage));
#endif
	}

	void OpenGLVertexBuffer::Bind() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
	}

	void OpenGLVertexBuffer::Unbind() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void* OpenGLVertexBuffer::GetPointer(uint32_t size) const
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
#ifdef MC_WEB
		return glMapBufferRange(GL_ARRAY_BUFFER, 0, size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);                                                                            
#else
		return glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
#endif
	}

	void OpenGLVertexBuffer::FreePointer() const
	{
		glUnmapBuffer(GL_ARRAY_BUFFER);
	}

}
