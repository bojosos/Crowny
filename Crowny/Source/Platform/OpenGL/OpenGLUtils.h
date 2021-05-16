#pragma once

#include <glad/glad.h>

namespace Crowny
{
    
    class OpenGLUtils
    {
    public:
        static uint32_t BufferUsageToOpenGLBufferUsage(BufferUsage usage);
    };
    
}