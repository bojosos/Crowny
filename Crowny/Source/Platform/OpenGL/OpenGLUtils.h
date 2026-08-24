#pragma once

#include "Crowny/Common/Types.h"

#include <glad/glad.h>

namespace Crowny
{
    struct OpenGLTextureFormat
    {
        GLenum InternalFormat = GL_NONE;
        GLenum TransferFormat = GL_NONE;
        GLenum TransferType = GL_NONE;
        bool Compressed = false;
    };

    class OpenGLUtils
    {
    public:
        static GLenum BufferUsageToOpenGLBufferUsage(BufferUsage usage);
        static GLbitfield LockOptionsToMapFlags(GpuLockOptions options);
        static GLenum DrawModeToOpenGL(DrawMode mode);
        static GLenum CompareFunctionToOpenGL(CompareFunction function);
        static GLenum StencilOperationToOpenGL(StencilOperation operation);
        static GLenum BlendFactorToOpenGL(BlendFactor factor);
        static GLenum BlendFunctionToOpenGL(BlendFunction function);
        static GLenum TextureTargetToOpenGL(TextureShape shape, uint32_t samples = 1);
        static OpenGLTextureFormat TextureFormatToOpenGL(TextureFormat format, bool sRGB);
        static uint32_t FlattenBinding(uint32_t set, uint32_t slot) { return set * 16 + slot; }
    };
} // namespace Crowny
