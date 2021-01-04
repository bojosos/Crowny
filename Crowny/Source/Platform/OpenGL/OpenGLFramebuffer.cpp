#include "cwpch.h"

#include "Platform/OpenGL/OpenGLFramebuffer.h"
#include "Crowny/Renderer/Texture.h"

#include <glad/glad.h>

namespace Crowny
{
	
	static TextureFormat FramebufferToTextureFormat(FramebufferTextureFormat textureFormat)
	{
		switch(textureFormat)
		{
			case FramebufferTextureFormat::R32I:			    return TextureFormat::R32I;
			case FramebufferTextureFormat::RGB8:    			return TextureFormat::RGB8;
			case FramebufferTextureFormat::RG32F:   			return TextureFormat::RG32F;
			case FramebufferTextureFormat::RGBA16F: 			return TextureFormat::RGBA16F;
			case FramebufferTextureFormat::RGBA32F:		  		return TextureFormat::RGBA32F;
			case FramebufferTextureFormat::DEPTH24STENCIL8: 	return TextureFormat::DEPTH24STENCIL8;
			case FramebufferTextureFormat::DEPTH32F:        	return TextureFormat::DEPTH32F;
			case FramebufferTextureFormat::RGBA8: 				return TextureFormat::RGBA8;
			default: return TextureFormat::NONE;
		}
		
		return TextureFormat::NONE;
	}
	
	OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferProperties& props) : m_Properties(props)
	{
		Invalidate();
	}

	void OpenGLFramebuffer::Invalidate()
	{
		if (m_RendererID)
		{
			glDeleteFramebuffers(1, &m_RendererID);
		}

		glCreateFramebuffers(1, &m_RendererID);
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
		auto& attachments = m_Properties.Attachments.Attachments;
		m_Attachments.resize(attachments.size());
		for (int i = 0; i < attachments.size(); i++)
		{
			m_Attachments[i] = Texture2D::Create(m_Properties.Width, m_Properties.Height, 
													 { FramebufferToTextureFormat(attachments[i].TextureFormat)} );
			if (attachments[i].TextureFormat == FramebufferTextureFormat::DEPTH24STENCIL8)
			{
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_Attachments[i]->GetRendererID(), 0);
			}
			else if (attachments[i].TextureFormat == FramebufferTextureFormat::DEPTH32F)
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_Attachments[i]->GetRendererID(), 0);
			else
			{
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, m_Attachments[i]->GetRendererID(), 0);
			}		
		}

		CW_ENGINE_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Framebuffer is incomplete!");
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	OpenGLFramebuffer::~OpenGLFramebuffer()
	{
		glDeleteFramebuffers(1, &m_RendererID);
	}

	void OpenGLFramebuffer::Bind() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
		glViewport(0, 0, m_Properties.Width, m_Properties.Height);
	}

	void OpenGLFramebuffer::Unbind() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void OpenGLFramebuffer::Resize(uint32_t width, uint32_t height)
	{
		m_Properties.Width = width;
		m_Properties.Height = height;

		Invalidate();
	}
}