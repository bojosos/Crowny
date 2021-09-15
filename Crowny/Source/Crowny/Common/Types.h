#pragma once

#include <glm/glm.hpp>

namespace Crowny
{

    using byte = uint8_t;

    struct Rect2F
    {
        float X, Y, Width, Height;

        Rect2F() : X(0), Y(0), Width(0), Height(0) {}
        Rect2F(float x, float y) : X(x), Y(y), Width(0), Height(0) {}
        Rect2F(float x, float y, float width, float height) : X(x), Y(y), Width(width), Height(height) {}

        bool Contains(const glm::vec2& pos)
        {
            return (X <= pos.x && Y <= pos.y && X + Width >= pos.x && Y + Height >= pos.y);
        }

        bool operator==(const Rect2F& other)
        {
            return X == other.X && Y == other.Y && Width == other.Width && Height == other.Height;
        }
    };

    struct Rect2I
    {
        int32_t X, Y, Width, Height;

        Rect2I() : X(0), Y(0), Width(0), Height(0) {}
        Rect2I(int32_t x, int32_t y) : X(x), Y(y), Width(0), Height(0) {}
        Rect2I(int32_t x, int32_t y, int32_t width, int32_t height) : X(x), Y(y), Width(width), Height(height) {}
    };

    struct Padding
    {
        float Left, Right, Top, Bottom;

        Padding() : Left(0), Right(0), Top(0), Bottom(0) {}
        Padding(float padding) : Left(padding), Right(padding), Top(padding), Bottom(padding) {}
        Padding(float vertical, float horizontal) : Left(horizontal), Right(horizontal), Top(vertical), Bottom(vertical)
        {
        }
        Padding(float top, float right, float bottom, float left) : Top(top), Left(left), Right(right), Bottom(bottom)
        {
        }
    };

    struct TextureSurface
    {
        TextureSurface(uint32_t mipLevel = 0, uint32_t numMipLevels = 1, uint32_t face = 0, uint32_t numFaces = 1)
          : MipLevel(mipLevel), NumMipLevels(numMipLevels), Face(face), NumFaces(numFaces)
        {
        }

        uint32_t MipLevel;
        uint32_t NumMipLevels;
        uint32_t Face;
        uint32_t NumFaces;

        static const TextureSurface COMPLETE;
    };

    enum class BlendFunction
    {
        ADD,
        SUBTRACT,
        MIN,
        MAX,
        REVERSE_SUBTRACT
    };

    enum class CompareFunction
    {
        ALWAYS_FAIL,
        ALWAYS_PASS,
        LESS,
        LESS_EQUAL,
        EQUAL,
        NOT_EQUAL,
        GREATER,
        GREATER_EQUAL,
    };

    enum class CullingMode
    {
        CULL_NONE,
        CULL_CLOCKWISE,
        CULL_COUNTERCLOCKWISE
    };

    enum class TextureFormat
    {
        NONE = 0,

        R8 = 1,
        RG8 = 2,
        RGB8 = 3,
        RGBA8 = 4,

        RGBA16F = 5,
        RGB32F = 6,
        RGBA32F = 7,
        RG16F = 8,
        RG32F = 9,
        R32I = 10,
        DEPTH32F = 11,
        DEPTH24STENCIL8 = 12
    };

    enum class TextureChannel
    {
        NONE,
        CHANNEL_RED,
        CHANNEL_RG,
        CHANNEL_RGB,
        CHANNEL_BGR,
        CHANNEL_RGBA,
        CHANNEL_BGRA,
        CHANNEL_DEPTH_COMPONENT,
        CHANNEL_STENCIL_INDEX
    };

    enum class TextureWrap
    {
        NONE = 0,
        REPEAT,
        MIRRORED_REPEAT,
        CLAMP_TO_EDGE,
        CLAMP_TO_BORDER
    };

    struct TextureAddressingMode
    {
        TextureWrap U, V, W;
    };

    enum class TextureFilter
    {
        NONE = 0,
        LINEAR,
        NEAREST,
        ANISOTROPIC
    };

    enum class SwizzleType
    {
        NONE = 0,
        SWIZZLE_RGBA,
        SWIZZLE_R,
        SWIZZLE_G,
        SWIZZLE_B,
        SWIZZLE_A
    };

    enum class SwizzleChannel
    {
        NONE = 0,
        RED,
        GREEN,
        BLUE,
        ALPHA,
        ONE,
        ZERO
    };

    enum class TextureShape
    {
        TEXTURE_1D,
        TEXTURE_2D,
        TEXTURE_3D,
        TEXTURE_CUBE
    };

    enum class TextureType
    {
        TEXTURE_SPRITE,
        TEXTURE_DEFAULT,
        TEXTURE_CURSOR,
        TEXTURE_NORMAL,
        TEXTURE_LIGHTMAP,
        TEXTURE_SINGLECHANNEL
    };

    enum TextureUsage // should not be here?
    {
        TEXTURE_STATIC = 1 << 0,
        TEXTURE_LOADSTORE = 1 << 1,
        TEXTURE_RENDERTARGET = 1 << 2,
        TEXTURE_DEPTHSTENCIL = 1 << 3,
        TEXTURE_DYNAMIC = 1 << 4
    };

    // TODO: refactor these enums, make same style
    enum ShaderType
    {
        VERTEX_SHADER,
        FRAGMENT_SHADER,
        GEOMETRY_SHADER,
        DOMAIN_SHADER,
        HULL_SHADER,
        COMPUTE_SHADER,
        SHADER_COUNT
    };

    enum GpuBufferUsage
    {
        STATIC_DRAW,
        DYNAMIC_DRAW
    };

    enum GpuQueueType
    {
        GRAPHICS_QUEUE,
        COMPUTE_QUEUE,
        UPLOAD_QUEUE,
        QUEUE_COUNT
    };

    enum class IndexType
    {
        Index_16,
        Index_32
    };

    enum class BufferUsage
    {
        STATIC_DRAW,
        DYNAMIC_DRAW
    };

    enum RenderSurfaceMaskBits
    {
        RT_NONE = 0,
        RT_COLOR0 = 1 << 0,
        RT_COLOR1 = 1 << 1,
        RT_COLOR2 = 1 << 2,
        RT_COLOR3 = 1 << 3,
        RT_COLOR4 = 1 << 4,
        RT_COLOR5 = 1 << 5,
        RT_COLOR6 = 1 << 6,
        RT_COLOR7 = 1 << 7,
        RT_DEPTH = 1 << 30,
        RT_STENCIL = 1 << 31,
        RT_DEPTH_STENCIL = (1 << 30) | (1 << 31),
        RT_ALL = 0xFFFFFFFF
    };
    typedef Flags<RenderSurfaceMaskBits> RenderSurfaceMask;
    CW_FLAGS_OPERATORS(RenderSurfaceMaskBits);

    enum FramebufferType
    {
        FBT_COLOR = 1 << 0,
        FBT_DEPTH = 1 << 1,
        FBT_STENCIL = 1 << 2
    };

    enum class GpuLockOptions
    {
        READ_ONLY,
        READ_WRITE,
        WRITE_ONLY,
        WRITE_ONLY_NO_OVERWRITE,
        WRITE_DISCARD,
        WRITE_DISCARD_RANGE
    };

    enum BufferWriteOptions
    {
        BWT_NORMAL,
        BWT_DISCARD,
        BWT_NO_OVERWRITE
    };

    enum class DrawMode
    {
        POINT_LIST,
        LINE_LIST,
        LINE_STRIP,
        TRIANGLE_LIST,
        TRIANGLE_STRIP,
        TRIANGLE_FAN
    };

    enum UniformResourceType
    {
        SAMPLER1D = 1,
        SAMPLER2D = 2,
        SAMPLER3D = 3,
        SAMPLERCUBE = 4,
        TEXTURE1D = 5,
        TEXTURE2D = 6,
        TEXTURE3D = 7,
        TEXTURECUBE = 8,
        // RWTEXTURE1D = 9,
        // RWTEXTURE2D = 10,
        // RWTEXTURE3D = 11,
        TEXTURE_UNKNOWN = 256
    };

    enum GpuBufferFormat
    {
        BF_UNKNOWN,
        BF_16x1F, // 1D 16-bit float
        BF_16X2F, // 2D 16-bit float
        BF_32x1F, // 1D 32-bit float
        BF_32x2F, // 2D 32-bit float
        BF_32x3F, // 3D 32-bit float
        BF_32x4F, // 4D 32-bit float

        BF_8X1,  // 1D 8-bit normalized
        BF_8X2,  // 2D 8-bit normalized
        BF_8X4,  // 4D 8-bit normalized
        BF_16X1, // 1D 16-bit normalized
        BF_16X2, // 2D 16-bit normalized
        BF_16X4, // 4D 16-bit normalized

        BF_8X1U, // 1D 8-bit unsigned int
        BF_8X2U, // 2D 8-bit unsigned int
        BF_8X4U, // 4D 8-bit unsigned int

        BF_16X1U, // 1D 16-bit unsigned int
        BF_16X2U, // 2D 16-bit unsigned int
        BF_16X4U, // 4D 16-bit unsigned int

        BF_32X1U, // 1D 32-bit unsigned int
        BF_32X2U, // 2D 32-bit unsigned int
        BF_32X3U, // 3D 32-bit unsigned int
        BF_32X4U, // 4D 32-bit unsigned int
    };

} // namespace Crowny
