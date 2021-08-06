#include "cwpch.h"

#include "Platform/OpenGL/OpenGLUtils.h"

namespace Crowny
{

    uint32_t OpenGLUtils::BufferUsageToOpenGLBufferUsage(BufferUsage usage)
    {
        switch (usage)
        {
        case (BufferUsage::STATIC_DRAW):
            return GL_STATIC_DRAW;
        case (BufferUsage::DYNAMIC_DRAW):
            return GL_DYNAMIC_DRAW;
        }
        CW_ENGINE_ASSERT(false, "Drawing mode not support");
        return GL_NONE;
    }

} // namespace Crowny