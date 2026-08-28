#include "cwpch.h"

#include "Platform/OpenGL/OpenGLUtils.h"

namespace Crowny
{
    namespace
    {
        constexpr GLenum COMPRESSED_RGB_S3TC_DXT1 = 0x83F0;
        constexpr GLenum COMPRESSED_RGBA_S3TC_DXT1 = 0x83F1;
        constexpr GLenum COMPRESSED_RGBA_S3TC_DXT3 = 0x83F2;
        constexpr GLenum COMPRESSED_RGBA_S3TC_DXT5 = 0x83F3;
        constexpr GLenum COMPRESSED_SRGB_S3TC_DXT1 = 0x8C4C;
        constexpr GLenum COMPRESSED_SRGB_ALPHA_S3TC_DXT1 = 0x8C4D;
        constexpr GLenum COMPRESSED_SRGB_ALPHA_S3TC_DXT3 = 0x8C4E;
        constexpr GLenum COMPRESSED_SRGB_ALPHA_S3TC_DXT5 = 0x8C4F;
        constexpr GLenum COMPRESSED_RGBA_ASTC_4X4 = 0x93B0;
        constexpr GLenum COMPRESSED_SRGB8_ALPHA8_ASTC_4X4 = 0x93D0;
    } // namespace

    GLenum OpenGLUtils::BufferUsageToOpenGLBufferUsage(BufferUsage usage)
    {
        if ((usage & BufferUsage::BU_DYNAMIC_DRAW) != 0)
            return GL_DYNAMIC_DRAW;
        if ((usage & BufferUsage::BU_LOADSTORE) == BufferUsage::BU_LOADSTORE)
            return GL_DYNAMIC_COPY;
        if ((usage & BufferUsage::BU_STATIC_DRAW) != 0)
            return GL_STATIC_DRAW;
        CW_ENGINE_ASSERT(false, "Unsupported OpenGL buffer usage");
        return GL_NONE;
    }

    GLbitfield OpenGLUtils::LockOptionsToMapFlags(GpuLockOptions options)
    {
        switch (options)
        {
        case GpuLockOptions::READ_ONLY: return GL_MAP_READ_BIT;
        case GpuLockOptions::READ_WRITE: return GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
        case GpuLockOptions::WRITE_ONLY:
        case GpuLockOptions::WRITE_ONLY_NO_OVERWRITE: return GL_MAP_WRITE_BIT;
        case GpuLockOptions::WRITE_DISCARD: return GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT;
        case GpuLockOptions::WRITE_DISCARD_RANGE: return GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT;
        }
        CW_ENGINE_ASSERT(false, "Unsupported OpenGL lock option");
        return 0;
    }

    GLenum OpenGLUtils::DrawModeToOpenGL(DrawMode mode)
    {
        switch (mode)
        {
        case DrawMode::POINT_LIST: return GL_POINTS;
        case DrawMode::LINE_LIST: return GL_LINES;
        case DrawMode::LINE_STRIP: return GL_LINE_STRIP;
        case DrawMode::TRIANGLE_LIST: return GL_TRIANGLES;
        case DrawMode::TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
        case DrawMode::TRIANGLE_FAN: return GL_TRIANGLE_FAN;
        }
        CW_ENGINE_ASSERT(false, "Unsupported OpenGL draw mode");
        return GL_NONE;
    }

    GLenum OpenGLUtils::CompareFunctionToOpenGL(CompareFunction function)
    {
        switch (function)
        {
        case CompareFunction::ALWAYS_FAIL: return GL_NEVER;
        case CompareFunction::ALWAYS_PASS: return GL_ALWAYS;
        case CompareFunction::LESS: return GL_LESS;
        case CompareFunction::LESS_EQUAL: return GL_LEQUAL;
        case CompareFunction::EQUAL: return GL_EQUAL;
        case CompareFunction::NOT_EQUAL: return GL_NOTEQUAL;
        case CompareFunction::GREATER: return GL_GREATER;
        case CompareFunction::GREATER_EQUAL: return GL_GEQUAL;
        }
        CW_ENGINE_ASSERT(false, "Unsupported OpenGL comparison function");
        return GL_ALWAYS;
    }

    GLenum OpenGLUtils::StencilOperationToOpenGL(StencilOperation operation)
    {
        switch (operation)
        {
        case StencilOperation::Keep: return GL_KEEP;
        case StencilOperation::Zero: return GL_ZERO;
        case StencilOperation::Replace: return GL_REPLACE;
        case StencilOperation::Increment: return GL_INCR;
        case StencilOperation::Decrement: return GL_DECR;
        case StencilOperation::IncrementWrap: return GL_INCR_WRAP;
        case StencilOperation::DecrementWrap: return GL_DECR_WRAP;
        case StencilOperation::Invert: return GL_INVERT;
        }
        CW_ENGINE_ASSERT(false, "Unsupported OpenGL stencil operation");
        return GL_KEEP;
    }

    GLenum OpenGLUtils::BlendFactorToOpenGL(BlendFactor factor)
    {
        switch (factor)
        {
        case BlendFactor::One: return GL_ONE;
        case BlendFactor::Zero: return GL_ZERO;
        case BlendFactor::DestColor: return GL_DST_COLOR;
        case BlendFactor::SourceColor: return GL_SRC_COLOR;
        case BlendFactor::InvDestColor: return GL_ONE_MINUS_DST_COLOR;
        case BlendFactor::InvSourceColor: return GL_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DestAlpha: return GL_DST_ALPHA;
        case BlendFactor::SourceAlpha: return GL_SRC_ALPHA;
        case BlendFactor::InvDestAlpha: return GL_ONE_MINUS_DST_ALPHA;
        case BlendFactor::InvSourceAlpha: return GL_ONE_MINUS_SRC_ALPHA;
        }
        CW_ENGINE_ASSERT(false, "Unsupported OpenGL blend factor");
        return GL_ONE;
    }

    GLenum OpenGLUtils::BlendFunctionToOpenGL(BlendFunction function)
    {
        switch (function)
        {
        case BlendFunction::ADD: return GL_FUNC_ADD;
        case BlendFunction::SUBTRACT: return GL_FUNC_SUBTRACT;
        case BlendFunction::MIN: return GL_MIN;
        case BlendFunction::MAX: return GL_MAX;
        case BlendFunction::REVERSE_SUBTRACT: return GL_FUNC_REVERSE_SUBTRACT;
        }
        CW_ENGINE_ASSERT(false, "Unsupported OpenGL blend function");
        return GL_FUNC_ADD;
    }

    GLenum OpenGLUtils::TextureTargetToOpenGL(TextureShape shape, uint32_t samples)
    {
        switch (shape)
        {
        case TextureShape::TEXTURE_1D: return GL_TEXTURE_1D;
        case TextureShape::TEXTURE_2D: return samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        case TextureShape::TEXTURE_3D: return GL_TEXTURE_3D;
        case TextureShape::TEXTURE_CUBE: return GL_TEXTURE_CUBE_MAP;
        }
        CW_ENGINE_ASSERT(false, "Unsupported OpenGL texture shape");
        return GL_NONE;
    }

    OpenGLTextureFormat OpenGLUtils::TextureFormatToOpenGL(TextureFormat format, bool sRGB)
    {
        switch (format)
        {
        case TextureFormat::R8: return { GL_R8, GL_RED, GL_UNSIGNED_BYTE, false };
        case TextureFormat::RG8: return { GL_RG8, GL_RG, GL_UNSIGNED_BYTE, false };
        case TextureFormat::RGB8: return { static_cast<GLenum>(sRGB ? GL_SRGB8 : GL_RGB8), GL_RGB, GL_UNSIGNED_BYTE, false };
        case TextureFormat::RGBA8: return { static_cast<GLenum>(sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8), GL_RGBA, GL_UNSIGNED_BYTE, false };
        case TextureFormat::BGRA8: return { static_cast<GLenum>(sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8), GL_BGRA, GL_UNSIGNED_BYTE, false };
        case TextureFormat::RG16F: return { GL_RG16F, GL_RG, GL_HALF_FLOAT, false };
        case TextureFormat::RGBA16F: return { GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false };
        case TextureFormat::RGB32F: return { GL_RGB32F, GL_RGB, GL_FLOAT, false };
        case TextureFormat::RG32F: return { GL_RG32F, GL_RG, GL_FLOAT, false };
        case TextureFormat::RGBA32F: return { GL_RGBA32F, GL_RGBA, GL_FLOAT, false };
        case TextureFormat::R32I: return { GL_R32I, GL_RED_INTEGER, GL_INT, false };
        case TextureFormat::R32F: return { GL_R32F, GL_RED, GL_FLOAT, false };
        case TextureFormat::R16: return { GL_R16, GL_RED, GL_UNSIGNED_SHORT, false };
        case TextureFormat::RG16: return { GL_RG16, GL_RG, GL_UNSIGNED_SHORT, false };
        case TextureFormat::RGB16: return { GL_RGB16, GL_RGB, GL_UNSIGNED_SHORT, false };
        case TextureFormat::RGBA16: return { GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, false };
        case TextureFormat::DEPTH32F: return { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, false };
        case TextureFormat::DEPTH24STENCIL8: return { GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, false };
        case TextureFormat::BC1:
            return { sRGB ? COMPRESSED_SRGB_S3TC_DXT1 : COMPRESSED_RGB_S3TC_DXT1, GL_NONE, GL_NONE, true };
        case TextureFormat::BC1a:
            return { sRGB ? COMPRESSED_SRGB_ALPHA_S3TC_DXT1 : COMPRESSED_RGBA_S3TC_DXT1, GL_NONE, GL_NONE, true };
        case TextureFormat::BC2:
            return { sRGB ? COMPRESSED_SRGB_ALPHA_S3TC_DXT3 : COMPRESSED_RGBA_S3TC_DXT3, GL_NONE, GL_NONE, true };
        case TextureFormat::BC3:
            return { sRGB ? COMPRESSED_SRGB_ALPHA_S3TC_DXT5 : COMPRESSED_RGBA_S3TC_DXT5, GL_NONE, GL_NONE, true };
        case TextureFormat::BC4: return { GL_COMPRESSED_RED_RGTC1, GL_NONE, GL_NONE, true };
        case TextureFormat::BC5: return { GL_COMPRESSED_RG_RGTC2, GL_NONE, GL_NONE, true };
        case TextureFormat::BC6H: return { GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT, GL_NONE, GL_NONE, true };
        case TextureFormat::BC7:
            return { static_cast<GLenum>(sRGB ? GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM : GL_COMPRESSED_RGBA_BPTC_UNORM), GL_NONE, GL_NONE, true };
        case TextureFormat::ETC2_RGB:
            return { static_cast<GLenum>(sRGB ? GL_COMPRESSED_SRGB8_ETC2 : GL_COMPRESSED_RGB8_ETC2), GL_NONE, GL_NONE, true };
        case TextureFormat::ETC2_RGBA:
            return { static_cast<GLenum>(sRGB ? GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC : GL_COMPRESSED_RGBA8_ETC2_EAC), GL_NONE, GL_NONE, true };
        case TextureFormat::ETC2_R11: return { GL_COMPRESSED_R11_EAC, GL_NONE, GL_NONE, true };
        case TextureFormat::ETC2_RG11: return { GL_COMPRESSED_RG11_EAC, GL_NONE, GL_NONE, true };
        case TextureFormat::ASTC4x4:
            return { sRGB ? COMPRESSED_SRGB8_ALPHA8_ASTC_4X4 : COMPRESSED_RGBA_ASTC_4X4, GL_NONE, GL_NONE, true };
        case TextureFormat::NONE:
        case TextureFormat::FormatCount: break;
        }
        CW_ENGINE_ASSERT(false, "Unsupported OpenGL texture format");
        return {};
    }
} // namespace Crowny
