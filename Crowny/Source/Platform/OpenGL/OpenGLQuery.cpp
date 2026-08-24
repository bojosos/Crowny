#include "cwpch.h"

#include "Platform/OpenGL/OpenGLQuery.h"
#include "Platform/OpenGL/OpenGLCommandBuffer.h"

#include <glad/glad.h>

#include <stdexcept>

namespace Crowny
{
    namespace
    {
        void RequireImmediate(const Ref<CommandBuffer>& commandBuffer)
        {
            if (commandBuffer && dynamic_cast<OpenGLCommandBuffer*>(commandBuffer.get()) == nullptr)
                throw std::invalid_argument("OpenGL queries cannot use a command buffer from another rendering backend");
        }
    } // namespace

    OpenGLTimerQuery::OpenGLTimerQuery()
    {
        m_Active = false;
        glGenQueries(1, &m_Query);
    }

    OpenGLTimerQuery::~OpenGLTimerQuery()
    {
        if (m_Query != 0)
            glDeleteQueries(1, &m_Query);
    }

    void OpenGLTimerQuery::Begin(const Ref<CommandBuffer>& cb)
    {
        RequireImmediate(cb);
        CW_ENGINE_ASSERT(!m_Active, "OpenGL timer query is already active");
        glBeginQuery(GL_TIME_ELAPSED, m_Query);
        m_Active = true;
    }

    void OpenGLTimerQuery::End(const Ref<CommandBuffer>& cb)
    {
        RequireImmediate(cb);
        CW_ENGINE_ASSERT(m_Active, "OpenGL timer query is not active");
        glEndQuery(GL_TIME_ELAPSED);
        m_Active = false;
    }

    bool OpenGLTimerQuery::IsReady() const
    {
        if (m_Active)
            return false;
        GLint available = GL_FALSE;
        glGetQueryObjectiv(m_Query, GL_QUERY_RESULT_AVAILABLE, &available);
        return available == GL_TRUE;
    }

    float OpenGLTimerQuery::GetTimeMs()
    {
        CW_ENGINE_ASSERT(!m_Active, "Cannot read an active OpenGL timer query");
        GLuint64 nanoseconds = 0;
        glGetQueryObjectui64v(m_Query, GL_QUERY_RESULT, &nanoseconds);
        return static_cast<float>(static_cast<double>(nanoseconds) / 1000000.0);
    }

    OpenGLPipelineQuery::OpenGLPipelineQuery()
    {
        SetActive(false);
        glGenQueries(1, &m_Query);
    }

    OpenGLPipelineQuery::~OpenGLPipelineQuery()
    {
        if (m_Query != 0)
            glDeleteQueries(1, &m_Query);
    }

    void OpenGLPipelineQuery::Begin(const Ref<CommandBuffer>& cb)
    {
        RequireImmediate(cb);
        CW_ENGINE_ASSERT(!IsActive(), "OpenGL pipeline query is already active");
        glBeginQuery(GL_PRIMITIVES_GENERATED, m_Query);
        SetActive(true);
    }

    void OpenGLPipelineQuery::End(const Ref<CommandBuffer>& cb)
    {
        RequireImmediate(cb);
        CW_ENGINE_ASSERT(IsActive(), "OpenGL pipeline query is not active");
        glEndQuery(GL_PRIMITIVES_GENERATED);
        SetActive(false);
    }

    bool OpenGLPipelineQuery::IsReady() const
    {
        if (IsActive())
            return false;
        GLint available = GL_FALSE;
        glGetQueryObjectiv(m_Query, GL_QUERY_RESULT_AVAILABLE, &available);
        return available == GL_TRUE;
    }

    OpenGLOcclusionQuery::OpenGLOcclusionQuery(bool binary) : m_Binary(binary) { glGenQueries(1, &m_Query); }

    OpenGLOcclusionQuery::~OpenGLOcclusionQuery()
    {
        if (m_Query != 0)
            glDeleteQueries(1, &m_Query);
    }
} // namespace Crowny
