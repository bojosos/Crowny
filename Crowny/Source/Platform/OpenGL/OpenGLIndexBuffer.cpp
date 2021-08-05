#include "cwpch.h"

#include "Platform/OpenGL/OpenGLIndexBuffer.h"
#include "Platform/OpenGL/OpenGLUtils.h"

#include <glad/glad.h>

namespace Crowny
{
	
	OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t count, IndexType indexType, BufferUsage usage) : m_Count(count), m_IndexType(indexType)
	{
		uint32_t indexSize = indexType == IndexType::Index_32 ? 32 : 16;
#ifdef MC_WEB
		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * indexSize, nullptr, OpenGLUtils::BufferUsageToOpenGLBufferUsage(usage));
#else
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, count * indexSize, nullptr, OpenGLUtils::BufferUsageToOpenGLBufferUsage(usage));
#endif
	}

	OpenGLIndexBuffer::OpenGLIndexBuffer(uint16_t* indices, uint32_t count, BufferUsage usage) : m_Count(count), m_IndexType(IndexType::Index_16)
	{
#ifdef MC_WEB
		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint16_t), indices, OpenGLUtils::BufferUsageToOpenGLBufferUsage(usage));
#else
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, count * sizeof(uint16_t), indices, OpenGLUtils::BufferUsageToOpenGLBufferUsage(usage));
#endif
	}
	
	OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t* indices, uint32_t count, BufferUsage usage) : m_Count(count), m_IndexType(IndexType::Index_32)
	{
#ifdef MC_WEB
		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, OpenGLUtils::BufferUsageToOpenGLBufferUsage(usage));
#else
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, count * sizeof(uint32_t), indices, OpenGLUtils::BufferUsageToOpenGLBufferUsage(usage));
#endif
	}

	OpenGLIndexBuffer::~OpenGLIndexBuffer()
	{
		glDeleteBuffers(1, &m_RendererID);
	}

	void OpenGLIndexBuffer::Bind() const
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
	}

	void OpenGLIndexBuffer::Unbind() const
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	void* OpenGLIndexBuffer::Map(uint32_t offset, uint32_t size, GpuLockOptions options)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
		switch(options)
		{
			case(GpuLockOptions::READ_ONLY): 			    return glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, offset, size, GL_MAP_READ_BIT);
			case(GpuLockOptions::WRITE_ONLY): 			    return glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, offset, size, GL_MAP_WRITE_BIT);
			case(GpuLockOptions::WRITE_DISCARD): 		    return glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, offset, size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
			case(GpuLockOptions::WRITE_DISCARD_RANGE):      return glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, offset, size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
			case(GpuLockOptions::READ_WRITE):               return glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, offset, size, GL_MAP_WRITE_BIT | GL_MAP_READ_BIT);
			case(GpuLockOptions::WRITE_ONLY_NO_OVERWRITE):  return glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, offset, size, GL_MAP_WRITE_BIT);
		}
		return nullptr;
	}

	void OpenGLIndexBuffer::Unmap()
	{
		glUnmapNamedBuffer(m_RendererID); // TODO: es2, might work with emscripten
	}
}
