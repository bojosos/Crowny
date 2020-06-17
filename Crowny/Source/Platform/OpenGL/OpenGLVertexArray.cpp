#include "cwpch.h"

<<<<<<< HEAD
#include "Platform/OpenGL/OpenGLVertexArray.h"
=======
#include "OpenGLVertexArray.h"
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2

#ifdef MC_WEB
#include <GLES3/gl32.h>
#else
#include <glad/glad.h>
#endif

namespace Crowny
{

	static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type)
	{
		switch (type)
		{
		case ShaderDataType::Byte:     return GL_BYTE;
		case ShaderDataType::Byte2:    return GL_BYTE;
		case ShaderDataType::Byte3:    return GL_BYTE;
		case ShaderDataType::Byte4:	   return GL_BYTE;
		case ShaderDataType::UByte4:   return GL_UNSIGNED_BYTE;
		case ShaderDataType::Float:    return GL_FLOAT;
		case ShaderDataType::Float2:   return GL_FLOAT;
		case ShaderDataType::Float3:   return GL_FLOAT;
		case ShaderDataType::Float4:   return GL_FLOAT;
		case ShaderDataType::Mat3:     return GL_FLOAT;
		case ShaderDataType::Mat4:     return GL_FLOAT;
		case ShaderDataType::Int:      return GL_INT;
		case ShaderDataType::Int2:     return GL_INT;
		case ShaderDataType::Int3:     return GL_INT;
		case ShaderDataType::Int4:     return GL_INT;
		case ShaderDataType::Bool:     return GL_BOOL;
		}

		CW_ENGINE_ASSERT(false, "Unknown ShaderDataType!");
		return 0;
	}

	OpenGLVertexArray::OpenGLVertexArray()
	{
<<<<<<< HEAD
		glGenVertexArrays(1, &m_RendererID);
=======
		glGenOpenGLVertexArrays(1, &m_RendererID);
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2
	}

	OpenGLVertexArray::~OpenGLVertexArray()
	{
<<<<<<< HEAD
		glDeleteVertexArrays(1, &m_RendererID);
=======
		glDeleteOpenGLVertexArrays(1, &m_RendererID);
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2
	}

	void OpenGLVertexArray::Bind() const
	{
<<<<<<< HEAD
		glBindVertexArray(m_RendererID);
=======
		glBindOpenGLVertexArray(m_RendererID);
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2
		if(m_IndexBuffer)
			m_IndexBuffer->Bind();
	}

	void OpenGLVertexArray::Unbind() const
	{
<<<<<<< HEAD
		glBindVertexArray(0);
=======
		glBindOpenGLVertexArray(0);
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2
		if(m_IndexBuffer)
			m_IndexBuffer->Unbind();
	}

	void OpenGLVertexArray::AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer)
	{
		CW_ENGINE_ASSERT(vertexBuffer->GetLayout().GetElements().size(), "Vertex Buffer has no layout!");

<<<<<<< HEAD
		glBindVertexArray(m_RendererID);
=======
		glBindOpenGLVertexArray(m_RendererID);
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2
		vertexBuffer->Bind();

		const auto& layout = vertexBuffer->GetLayout();
		for (const auto& element : layout)
		{
			glEnableVertexAttribArray(m_VertexBufferIndex);
			glVertexAttribPointer(m_VertexBufferIndex,
				element.GetComponentCount(),
				ShaderDataTypeToOpenGLBaseType(element.Type),
				element.Normalized ? GL_TRUE : GL_FALSE,
				layout.GetStride(),
				(const void*)element.Offset);
			m_VertexBufferIndex++;
		}
		m_VertexBufferIndex = 0;
		m_VertexBuffers.push_back(vertexBuffer);
	}

	void OpenGLVertexArray::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer)
	{
<<<<<<< HEAD
		glBindVertexArray(m_RendererID);
=======
		glBindOpenGLVertexArray(m_RendererID);
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2
		indexBuffer->Bind();

		m_IndexBuffer = indexBuffer;
	}
}
