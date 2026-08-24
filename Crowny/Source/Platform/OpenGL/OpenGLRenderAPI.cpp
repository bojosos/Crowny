#include "cwpch.h"

#include "Platform/OpenGL/OpenGLRenderAPI.h"

#include "Platform/OpenGL/OpenGLIndexBuffer.h"
#include "Platform/OpenGL/OpenGLCommandBuffer.h"
#include "Platform/OpenGL/OpenGLGpuBuffer.h"
#include "Platform/OpenGL/OpenGLPipeline.h"
#include "Platform/OpenGL/OpenGLRenderTexture.h"
#include "Platform/OpenGL/OpenGLRenderWindow.h"
#include "Platform/OpenGL/OpenGLUniformParams.h"
#include "Platform/OpenGL/OpenGLUtils.h"
#include "Platform/OpenGL/OpenGLVertexBuffer.h"

#include <glad/glad.h>

#include <stdexcept>

namespace Crowny
{
    namespace
    {
        constexpr GLenum COMPRESSED_RGB_S3TC_DXT1 = 0x83F0;
        constexpr GLenum COMPRESSED_RGBA_S3TC_DXT5 = 0x83F3;
        constexpr GLenum COMPRESSED_RGBA_BPTC_UNORM = 0x8E8C;
        constexpr GLenum COMPRESSED_SRGB_ALPHA_BPTC_UNORM = 0x8E8D;
        constexpr GLenum COMPRESSED_RGBA_ASTC_4X4 = 0x93B0;

        bool SupportsCompressedFormat(GLenum format)
        {
            GLint count = 0;
            glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &count);
            if (count <= 0)
                return false;
            Vector<GLint> formats(static_cast<size_t>(count));
            glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, formats.data());
            return std::find(formats.begin(), formats.end(), static_cast<GLint>(format)) != formats.end();
        }

        void GLAPIENTRY OpenGLMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message,
                                              const void* userParam)
        {
            (void)source;
            (void)type;
            (void)length;
            (void)userParam;
            if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
                return;
            if (severity == GL_DEBUG_SEVERITY_HIGH)
                CW_ENGINE_CRITICAL("OpenGL [{}]: {}", id, message);
            else if (severity == GL_DEBUG_SEVERITY_MEDIUM)
                CW_ENGINE_ERROR("OpenGL [{}]: {}", id, message);
            else
                CW_ENGINE_WARN("OpenGL [{}]: {}", id, message);
        }

        GLenum AttributeBaseType(ShaderDataType type)
        {
            switch (type)
            {
            case ShaderDataType::Bool:
            case ShaderDataType::Int:
            case ShaderDataType::Int2:
            case ShaderDataType::Int3:
            case ShaderDataType::Int4: return GL_INT;
            case ShaderDataType::SByte:
            case ShaderDataType::SByte2:
            case ShaderDataType::SByte3:
            case ShaderDataType::SByte4: return GL_BYTE;
            case ShaderDataType::UByte4:
            case ShaderDataType::Color: return GL_UNSIGNED_BYTE;
            default: return GL_FLOAT;
            }
        }

        bool IsIntegerAttribute(ShaderDataType type)
        {
            return type == ShaderDataType::Bool || type == ShaderDataType::Int || type == ShaderDataType::Int2 ||
                   type == ShaderDataType::Int3 || type == ShaderDataType::Int4;
        }

        uint32_t AttributeComponentCount(ShaderDataType type)
        {
            if (type == ShaderDataType::Color)
                return 4;
            return type == ShaderDataType::Mat3 ? 3 : type == ShaderDataType::Mat4 ? 4 : BufferElement(type, VertexAttribute::None).GetComponentCount();
        }

        GLenum PolygonModeToOpenGL(PolygonMode mode)
        {
            switch (mode)
            {
            case PolygonMode::Wireframe: return GL_LINE;
            case PolygonMode::Points: return GL_POINT;
            case PolygonMode::Solid: return GL_FILL;
            }
            return GL_FILL;
        }
    } // namespace

    void OpenGLRenderAPI::Init()
    {
        if (glGetString(GL_VERSION) == nullptr)
            throw std::runtime_error("OpenGLRenderAPI::Init requires a current OpenGL context");

        glGenVertexArrays(1, &m_VertexArray);
        glBindVertexArray(m_VertexArray);
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

#if defined(CW_DEBUG)
        if (GLAD_GL_VERSION_4_3)
        {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(OpenGLMessageCallback, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
        }
#endif

        m_Capabilities.RenderAPIName = "OpenGL";
        m_Capabilities.DeviceName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const String vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        m_Capabilities.DeviceVendor = RenderCapabilities::VendorFromString(vendor);
        m_Capabilities.IntegratedGpu = m_Capabilities.DeviceVendor == GPU_INTEL;
        m_Capabilities.Conventions.YAxis = Conventions::Axis::Up;
        m_Capabilities.Conventions.MatrixOrder = Conventions::MatrixOrder::ColumnMajor;
        m_Capabilities.MinDepth = -1.0f;
        m_Capabilities.MaxDepth = 1.0f;

        GLint value = 0;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &value);
        m_Capabilities.NumTextureUnitsPerStage[FRAGMENT_SHADER] = static_cast<uint16_t>(std::max(value, 0));
        glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &value);
        m_Capabilities.NumTextureUnitsPerStage[VERTEX_SHADER] = static_cast<uint16_t>(std::max(value, 0));
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &value);
        m_Capabilities.NumCombinedTextureUnits = static_cast<uint16_t>(std::max(value, 0));
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &value);
        m_Capabilities.NumMultiRenderTargets = static_cast<uint16_t>(std::max(value, 0));
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &value);
        m_Capabilities.MaxBoundVertexBuffers = static_cast<uint16_t>(std::max(value, 0));

        if (GLAD_GL_VERSION_3_2)
            m_Capabilities.SetCapability(CW_GEOMETRY_SHADER);
        if (GLAD_GL_VERSION_4_0)
            m_Capabilities.SetCapability(CW_TESSELLATION_SHADER);
        if (GLAD_GL_VERSION_4_2)
            m_Capabilities.SetCapability(CW_LOAD_STORE);
        if (GLAD_GL_VERSION_4_3)
            m_Capabilities.SetCapability(CW_COMPUTE_SHADER);
        if (SupportsCompressedFormat(COMPRESSED_RGB_S3TC_DXT1) &&
            SupportsCompressedFormat(COMPRESSED_RGBA_S3TC_DXT5) &&
            SupportsCompressedFormat(GL_COMPRESSED_RED_RGTC1) && SupportsCompressedFormat(GL_COMPRESSED_RG_RGTC2))
            m_Capabilities.SetCapability(CW_TEXTURE_COMPRESSION_BC);
        if (SupportsCompressedFormat(COMPRESSED_RGBA_BPTC_UNORM) &&
            SupportsCompressedFormat(COMPRESSED_SRGB_ALPHA_BPTC_UNORM))
            m_Capabilities.SetCapability(CW_TEXTURE_COMPRESSION_BPTC);
        if (SupportsCompressedFormat(GL_COMPRESSED_RGB8_ETC2) && SupportsCompressedFormat(GL_COMPRESSED_RGBA8_ETC2_EAC) &&
            SupportsCompressedFormat(GL_COMPRESSED_R11_EAC) && SupportsCompressedFormat(GL_COMPRESSED_RG11_EAC))
            m_Capabilities.SetCapability(CW_TEXTURE_COMPRESSION_ETC2);
        if (SupportsCompressedFormat(COMPRESSED_RGBA_ASTC_4X4))
            m_Capabilities.SetCapability(CW_TEXTURE_COMPRESSION_ASTC);
        m_Capabilities.AddShaderProfile("glsl");
    }

    const RenderCapabilities& OpenGLRenderAPI::GetCapabilities(uint32_t deviceIndex) const
    {
        CW_ENGINE_ASSERT(deviceIndex == 0, "OpenGL exposes one logical renderer device");
        return m_Capabilities;
    }

    void OpenGLRenderAPI::RequireImmediate(const Ref<CommandBuffer>& commandBuffer)
    {
        if (commandBuffer && dynamic_cast<OpenGLCommandBuffer*>(commandBuffer.get()) == nullptr)
            throw std::invalid_argument("The OpenGL backend cannot execute a command buffer from another rendering backend");
    }

    void OpenGLRenderAPI::SetViewport(float x, float y, float width, float height, const Ref<CommandBuffer>& commandBuffer)
    {
        RequireImmediate(commandBuffer);
        m_Viewport = Rect2F(x, y, width, height);
        ApplyViewport();
    }

    void OpenGLRenderAPI::SetScissorRect(const Rect2I& rect, const Ref<CommandBuffer>& commandBuffer)
    {
        RequireImmediate(commandBuffer);
        glScissor(rect.X, rect.Y, rect.Width, rect.Height);
    }

    void OpenGLRenderAPI::SetClearColor(const glm::vec4& color)
    {
        m_ClearColor = color;
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void OpenGLRenderAPI::SwapBuffers(const Ref<RenderTarget>& renderTarget, uint32_t syncMask)
    {
        if (renderTarget)
            renderTarget->SwapBuffers(syncMask);
    }

    void OpenGLRenderAPI::SetGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer)
    {
        RequireImmediate(commandBuffer);
        m_GraphicsPipeline = pipeline;
        m_ComputePipeline = nullptr;
        glUseProgram(pipeline ? static_cast<OpenGLGraphicsPipeline*>(pipeline.get())->GetProgram() : 0);
        ApplyPipelineState();
    }

    void OpenGLRenderAPI::SetRayTracingPipeline(const Ref<RayTracingPipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer)
    {
        (void)pipeline;
        RequireImmediate(commandBuffer);
        CW_ENGINE_ERROR("The OpenGL backend does not support ray tracing pipelines");
    }

    void OpenGLRenderAPI::SetComputePipeline(const Ref<ComputePipeline>& pipeline, const Ref<CommandBuffer>& commandBuffer)
    {
        RequireImmediate(commandBuffer);
        m_ComputePipeline = pipeline;
        m_GraphicsPipeline = nullptr;
        glUseProgram(pipeline ? static_cast<OpenGLComputePipeline*>(pipeline.get())->GetProgram() : 0);
    }

    void OpenGLRenderAPI::SubmitCommandBuffer(const Ref<CommandBuffer>& commandBuffer, uint32_t syncMask)
    {
        (void)syncMask;
        RequireImmediate(commandBuffer);
    }

    void OpenGLRenderAPI::SetIndexBuffer(const Ref<IndexBuffer>& buffer, const Ref<CommandBuffer>& commandBuffer)
    {
        RequireImmediate(commandBuffer);
        m_IndexBuffer = buffer;
        glBindVertexArray(m_VertexArray);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer ? static_cast<OpenGLIndexBuffer*>(buffer.get())->GetRendererID() : 0);
    }

    void OpenGLRenderAPI::SetVertexBuffers(uint32_t idx, Ref<VertexBuffer>* buffers, uint32_t bufferCount,
                                           const Ref<CommandBuffer>& commandBuffer)
    {
        RequireImmediate(commandBuffer);
        CW_ENGINE_ASSERT(bufferCount == 0 || buffers != nullptr, "OpenGL vertex buffer array is null");
        if (m_VertexBuffers.size() < idx + bufferCount)
            m_VertexBuffers.resize(idx + bufferCount);
        for (uint32_t index = 0; index < bufferCount; ++index)
            m_VertexBuffers[idx + index] = buffers[index];
        ConfigureVertexArray();
    }

    void OpenGLRenderAPI::SetVertexLayout(const Ref<BufferLayout>& vertexLayout, const Ref<CommandBuffer>& commandBuffer)
    {
        RequireImmediate(commandBuffer);
        m_VertexLayout = vertexLayout;
        ConfigureVertexArray();
    }

    void OpenGLRenderAPI::ConfigureVertexArray()
    {
        if (!m_VertexLayout || m_VertexBuffers.empty())
            return;
        glBindVertexArray(m_VertexArray);
        for (uint32_t location = 0; location < m_EnabledAttributeCount; ++location)
            glDisableVertexAttribArray(location);
        m_EnabledAttributeCount = 0;

        uint32_t nextLocation = 0;
        for (const BufferElement& element : *m_VertexLayout)
        {
            const uint32_t stream = std::min<uint32_t>(element.StreamIdx, static_cast<uint32_t>(m_VertexBuffers.size() - 1));
            const Ref<VertexBuffer>& buffer = m_VertexBuffers[stream];
            CW_ENGINE_ASSERT(buffer != nullptr, "OpenGL vertex layout references an unset stream");
            glBindBuffer(GL_ARRAY_BUFFER, static_cast<OpenGLVertexBuffer*>(buffer.get())->GetRendererID());
            const uint32_t location = element.Location == UINT32_MAX ? nextLocation : element.Location;
            const uint32_t columns = element.Type == ShaderDataType::Mat3 ? 3 : element.Type == ShaderDataType::Mat4 ? 4 : 1;
            const uint32_t components = AttributeComponentCount(element.Type);
            for (uint32_t column = 0; column < columns; ++column)
            {
                const uint32_t attribute = location + column;
                const uintptr_t offset = element.Offset + column * components * sizeof(float);
                glEnableVertexAttribArray(attribute);
                if (IsIntegerAttribute(element.Type) && !element.Normalized)
                    glVertexAttribIPointer(attribute, components, AttributeBaseType(element.Type), m_VertexLayout->GetStride(stream),
                                           reinterpret_cast<const void*>(offset));
                else
                    glVertexAttribPointer(attribute, components, AttributeBaseType(element.Type), element.Normalized ? GL_TRUE : GL_FALSE,
                                          m_VertexLayout->GetStride(stream), reinterpret_cast<const void*>(offset));
                glVertexAttribDivisor(attribute, element.InstanceRate);
                m_EnabledAttributeCount = std::max(m_EnabledAttributeCount, attribute + 1);
            }
            nextLocation = std::max(nextLocation, location + columns);
        }
        if (m_IndexBuffer)
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<OpenGLIndexBuffer*>(m_IndexBuffer.get())->GetRendererID());
    }

    void OpenGLRenderAPI::ClearViewport(uint32_t buffers, const glm::vec4& color, float depth, uint16_t stencil, uint8_t targetMask,
                                        const Ref<CommandBuffer>& commandBuffer)
    {
        RequireImmediate(commandBuffer);
        ClearRenderTarget(buffers, color, depth, stencil, targetMask);
    }

    void OpenGLRenderAPI::ClearRenderTarget(uint32_t buffers, const glm::vec4& color, float depth, uint16_t stencil, uint8_t targetMask,
                                            const Ref<CommandBuffer>& commandBuffer)
    {
        RequireImmediate(commandBuffer);
        GLboolean oldDepthMask = GL_TRUE;
        GLint oldStencilFrontMask = 0;
        GLint oldStencilBackMask = 0;
        GLdouble oldClearDepth = 1.0;
        GLint oldClearStencil = 0;
        GLfloat oldClearColor[4] = {};
        GLboolean oldColorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
        glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthMask);
        glGetIntegerv(GL_STENCIL_WRITEMASK, &oldStencilFrontMask);
        glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, &oldStencilBackMask);
        glGetBooleanv(GL_COLOR_WRITEMASK, oldColorMask);
        glGetDoublev(GL_DEPTH_CLEAR_VALUE, &oldClearDepth);
        glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &oldClearStencil);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, oldClearColor);
        glDepthMask(GL_TRUE);
        glStencilMaskSeparate(GL_FRONT, 0xFFFFFFFF);
        glStencilMaskSeparate(GL_BACK, 0xFFFFFFFF);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        GLbitfield mask = 0;
        if ((buffers & FBT_COLOR) != 0)
        {
            glClearColor(color.r, color.g, color.b, color.a);
            mask |= GL_COLOR_BUFFER_BIT;
        }
        if ((buffers & FBT_DEPTH) != 0)
        {
            glClearDepth(depth);
            mask |= GL_DEPTH_BUFFER_BIT;
        }
        if ((buffers & FBT_STENCIL) != 0)
        {
            glClearStencil(stencil);
            mask |= GL_STENCIL_BUFFER_BIT;
        }
        if (targetMask == 0xFF)
            glClear(mask);
        else
        {
            if ((buffers & FBT_COLOR) != 0)
                for (uint32_t index = 0; index < 8; ++index)
                    if ((targetMask & (1U << index)) != 0)
                        glClearBufferfv(GL_COLOR, index, &color[0]);
            if ((buffers & FBT_DEPTH) != 0 && (buffers & FBT_STENCIL) != 0)
                glClearBufferfi(GL_DEPTH_STENCIL, 0, depth, stencil);
            else if ((buffers & FBT_DEPTH) != 0)
                glClearBufferfv(GL_DEPTH, 0, &depth);
            else if ((buffers & FBT_STENCIL) != 0)
            {
                const GLint value = stencil;
                glClearBufferiv(GL_STENCIL, 0, &value);
            }
        }
        glDepthMask(oldDepthMask);
        glStencilMaskSeparate(GL_FRONT, static_cast<GLuint>(oldStencilFrontMask));
        glStencilMaskSeparate(GL_BACK, static_cast<GLuint>(oldStencilBackMask));
        glColorMask(oldColorMask[0], oldColorMask[1], oldColorMask[2], oldColorMask[3]);
        glClearDepth(oldClearDepth);
        glClearStencil(oldClearStencil);
        glClearColor(oldClearColor[0], oldClearColor[1], oldClearColor[2], oldClearColor[3]);
    }

    void OpenGLRenderAPI::Draw(uint32_t vertexOffset, uint32_t vertexCount, uint32_t instanceCount, const Ref<CommandBuffer>& commandBuffer)
    {
        RequireImmediate(commandBuffer);
        glBindVertexArray(m_VertexArray);
        if (instanceCount > 1)
            glDrawArraysInstanced(OpenGLUtils::DrawModeToOpenGL(m_DrawMode), vertexOffset, vertexCount, instanceCount);
        else
            glDrawArrays(OpenGLUtils::DrawModeToOpenGL(m_DrawMode), vertexOffset, vertexCount);
        RecordDraw(m_DrawMode, vertexCount, instanceCount);
    }

    void OpenGLRenderAPI::DrawIndexed(uint32_t startIndex, uint32_t indexCount, uint32_t vertexOffset, uint32_t vertexCount,
                                      uint32_t instanceCount, const Ref<CommandBuffer>& commandBuffer)
    {
        (void)vertexCount;
        RequireImmediate(commandBuffer);
        CW_ENGINE_ASSERT(m_IndexBuffer != nullptr, "OpenGL indexed draw requires an index buffer");
        glBindVertexArray(m_VertexArray);
        const GLenum indexType = m_IndexBuffer->GetIndexType() == IndexType::Index_16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
        const uintptr_t byteOffset = startIndex * (indexType == GL_UNSIGNED_SHORT ? sizeof(uint16_t) : sizeof(uint32_t));
        if (instanceCount > 1)
            glDrawElementsInstancedBaseVertex(OpenGLUtils::DrawModeToOpenGL(m_DrawMode), indexCount, indexType,
                                              reinterpret_cast<const void*>(byteOffset), instanceCount, vertexOffset);
        else
            glDrawElementsBaseVertex(OpenGLUtils::DrawModeToOpenGL(m_DrawMode), indexCount, indexType,
                                     reinterpret_cast<const void*>(byteOffset), vertexOffset);
        RecordDraw(m_DrawMode, indexCount, instanceCount);
    }

    void OpenGLRenderAPI::DrawIndexedIndirect(const Ref<GenericGpuBuffer>& argumentBuffer, uint32_t argumentOffset, uint32_t drawCount,
                                              uint32_t stride, const Ref<CommandBuffer>& commandBuffer)
    {
        RequireImmediate(commandBuffer);
        CW_ENGINE_ASSERT(argumentBuffer != nullptr, "OpenGL indirect draw requires an argument buffer");
        CW_ENGINE_ASSERT(m_IndexBuffer != nullptr, "OpenGL indirect indexed draw requires an index buffer");
        CW_ENGINE_ASSERT(stride >= sizeof(DrawIndexedIndirectCommand) && (stride & 3u) == 0,
                         "OpenGL indirect draw stride is invalid");
        if (argumentBuffer == nullptr || m_IndexBuffer == nullptr || drawCount == 0)
            return;

        const uint64_t requiredSize = static_cast<uint64_t>(argumentOffset) + static_cast<uint64_t>(drawCount - 1u) * stride +
                                      sizeof(DrawIndexedIndirectCommand);
        CW_ENGINE_ASSERT(requiredSize <= argumentBuffer->GetBufferSize(), "OpenGL indirect draw range exceeds its argument buffer");
        if (requiredSize > argumentBuffer->GetBufferSize())
            return;

        glBindVertexArray(m_VertexArray);
        const GLenum mode = OpenGLUtils::DrawModeToOpenGL(m_DrawMode);
        const GLenum indexType = m_IndexBuffer->GetIndexType() == IndexType::Index_16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
        OpenGLGenericGpuBuffer* glBuffer = static_cast<OpenGLGenericGpuBuffer*>(argumentBuffer.get());

        if (glMultiDrawElementsIndirect != nullptr)
        {
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, glBuffer->GetRendererID());
            glMultiDrawElementsIndirect(mode, indexType, reinterpret_cast<const void*>(static_cast<uintptr_t>(argumentOffset)), drawCount, stride);
            RecordIndirectDraw(drawCount);
            return;
        }

        Vector<uint8_t> commandBytes(static_cast<size_t>(drawCount - 1u) * stride + sizeof(DrawIndexedIndirectCommand));
        argumentBuffer->ReadData(argumentOffset, static_cast<uint32_t>(commandBytes.size()), commandBytes.data());
        const uint32_t indexSize = indexType == GL_UNSIGNED_SHORT ? sizeof(uint16_t) : sizeof(uint32_t);
        for (uint32_t draw = 0; draw < drawCount; draw++)
        {
            const DrawIndexedIndirectCommand& indirect =
              *reinterpret_cast<const DrawIndexedIndirectCommand*>(commandBytes.data() + static_cast<size_t>(draw) * stride);
            if (indirect.InstanceCount == 0 || indirect.IndexCount == 0)
                continue;
            const void* indexOffset = reinterpret_cast<const void*>(static_cast<uintptr_t>(indirect.FirstIndex * indexSize));
            if (glDrawElementsInstancedBaseVertexBaseInstance != nullptr)
            {
                glDrawElementsInstancedBaseVertexBaseInstance(mode, indirect.IndexCount, indexType, indexOffset, indirect.InstanceCount,
                                                              indirect.VertexOffset, indirect.FirstInstance);
            }
            else
            {
                glDrawElementsInstancedBaseVertex(mode, indirect.IndexCount, indexType, indexOffset, indirect.InstanceCount,
                                                  indirect.VertexOffset);
            }
        }
        RecordIndirectDraw(drawCount);
    }

    void OpenGLRenderAPI::DrawIndexedIndirectCount(const Ref<GenericGpuBuffer>& argumentBuffer, uint32_t argumentOffset,
                                                   const Ref<GenericGpuBuffer>& countBuffer, uint32_t countOffset, uint32_t maxDrawCount,
                                                   uint32_t stride, const Ref<CommandBuffer>& commandBuffer)
    {
        RequireImmediate(commandBuffer);
        CW_ENGINE_ASSERT(countBuffer != nullptr && countOffset + sizeof(uint32_t) <= countBuffer->GetBufferSize(),
                         "OpenGL indirect count range exceeds its buffer");
        if (countBuffer == nullptr || countOffset + sizeof(uint32_t) > countBuffer->GetBufferSize() || maxDrawCount == 0)
            return;

        if (glMultiDrawElementsIndirectCount != nullptr)
        {
            CW_ENGINE_ASSERT(argumentBuffer != nullptr && m_IndexBuffer != nullptr, "OpenGL indirect draw state is incomplete");
            glBindVertexArray(m_VertexArray);
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, static_cast<OpenGLGenericGpuBuffer*>(argumentBuffer.get())->GetRendererID());
            glBindBuffer(GL_PARAMETER_BUFFER, static_cast<OpenGLGenericGpuBuffer*>(countBuffer.get())->GetRendererID());
            const GLenum indexType = m_IndexBuffer->GetIndexType() == IndexType::Index_16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
            glMultiDrawElementsIndirectCount(OpenGLUtils::DrawModeToOpenGL(m_DrawMode), indexType,
                                             reinterpret_cast<const void*>(static_cast<uintptr_t>(argumentOffset)), countOffset,
                                             maxDrawCount, stride);
            RecordIndirectDraw(0); // The GPU count buffer is deliberately not read back for statistics.
            return;
        }

        uint32_t drawCount = 0;
        countBuffer->ReadData(countOffset, sizeof(drawCount), &drawCount);
        DrawIndexedIndirect(argumentBuffer, argumentOffset, std::min(drawCount, maxDrawCount), stride, commandBuffer);
    }

    void OpenGLRenderAPI::TraceRays(uint32_t width, uint32_t height, const Ref<CommandBuffer>& commandBuffer)
    {
        (void)width;
        (void)height;
        RequireImmediate(commandBuffer);
        CW_ENGINE_ERROR("The OpenGL backend does not support ray tracing");
    }

    void OpenGLRenderAPI::DispatchCompute(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ, const Ref<CommandBuffer>& commandBuffer)
    {
        RequireImmediate(commandBuffer);
        if (!GLAD_GL_VERSION_4_3)
            throw std::runtime_error("OpenGL compute dispatch requires OpenGL 4.3 or newer");
        CW_ENGINE_ASSERT(m_ComputePipeline != nullptr, "OpenGL compute dispatch requires a compute pipeline");
        glDispatchCompute(groupsX, groupsY, groupsZ);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        RecordComputeDispatch();
    }

    void OpenGLRenderAPI::DispatchComputeIndirect(const Ref<GenericGpuBuffer>& argumentBuffer, uint32_t argumentOffset,
                                                  const Ref<CommandBuffer>& commandBuffer)
    {
        RequireImmediate(commandBuffer);
        CW_ENGINE_ASSERT(argumentBuffer != nullptr && argumentOffset + sizeof(DispatchIndirectCommand) <= argumentBuffer->GetBufferSize(),
                         "OpenGL indirect dispatch range exceeds its argument buffer");
        if (argumentBuffer == nullptr || argumentOffset + sizeof(DispatchIndirectCommand) > argumentBuffer->GetBufferSize())
            return;

        if (glDispatchComputeIndirect != nullptr)
        {
            CW_ENGINE_ASSERT(m_ComputePipeline != nullptr, "OpenGL indirect dispatch requires a compute pipeline");
            glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, static_cast<OpenGLGenericGpuBuffer*>(argumentBuffer.get())->GetRendererID());
            glDispatchComputeIndirect(argumentOffset);
            glMemoryBarrier(GL_ALL_BARRIER_BITS);
            RecordComputeDispatch();
            return;
        }

        DispatchIndirectCommand arguments;
        argumentBuffer->ReadData(argumentOffset, sizeof(arguments), &arguments);
        DispatchCompute(arguments.GroupCountX, arguments.GroupCountY, arguments.GroupCountZ, commandBuffer);
    }

    void OpenGLRenderAPI::SetRenderTarget(const Ref<RenderTarget>& target, uint32_t readOnlyFlags, RenderSurfaceMask loadMask,
                                          const Ref<CommandBuffer>& commandBuffer)
    {
        (void)loadMask;
        RequireImmediate(commandBuffer);
        m_RenderTarget = target;
        m_ReadOnlyFlags = readOnlyFlags;
        const Ref<OpenGLRenderWindow> window = DynamicRefCast<OpenGLRenderWindow>(target);
        if (window)
            window->GetContext()->MakeCurrent();
        const Ref<OpenGLRenderTexture> texture = DynamicRefCast<OpenGLRenderTexture>(target);
        glBindFramebuffer(GL_FRAMEBUFFER, texture ? texture->GetFramebuffer() : 0);
        ApplyViewport();
        ApplyPipelineState();
    }

    void OpenGLRenderAPI::SetDrawMode(DrawMode drawMode, const Ref<CommandBuffer>& commandBuffer)
    {
        RequireImmediate(commandBuffer);
        m_DrawMode = drawMode;
    }

    void OpenGLRenderAPI::SetUniforms(const Ref<UniformParams>& params, const Ref<CommandBuffer>& commandBuffer)
    {
        RequireImmediate(commandBuffer);
        if (params)
            static_cast<const OpenGLUniformParams*>(params.get())->Bind();
    }

    void OpenGLRenderAPI::ApplyPipelineState()
    {
        if (!m_GraphicsPipeline)
            return;
        const PipelineStateDesc& desc = static_cast<OpenGLGraphicsPipeline*>(m_GraphicsPipeline.get())->GetDesc();
        const Ref<DepthStencilStateDesc> depth = desc.DepthStencilState ? desc.DepthStencilState : DepthStencilStateDesc::GetDefault();
        const Ref<RasterizerStateDesc> raster = desc.RasterizerState ? desc.RasterizerState : RasterizerStateDesc::GetDefault();
        const Ref<BlendStateDesc> blend = desc.BlendState ? desc.BlendState : BlendStateDesc::GetDefault();

        if (depth->EnableDepthRead)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
        glDepthMask(depth->EnableDepthWrite && (m_ReadOnlyFlags & FBT_DEPTH) == 0 ? GL_TRUE : GL_FALSE);
        glDepthFunc(OpenGLUtils::CompareFunctionToOpenGL(depth->DepthCompareFunction));

        if (depth->EnableStencil)
        {
            glEnable(GL_STENCIL_TEST);
            glStencilMask(depth->StencilWriteMask);
            glStencilFuncSeparate(GL_FRONT, OpenGLUtils::CompareFunctionToOpenGL(depth->StencilFrontCompare), 0, depth->StencilReadMask);
            glStencilFuncSeparate(GL_BACK, OpenGLUtils::CompareFunctionToOpenGL(depth->StencilBackCompare), 0, depth->StencilReadMask);
            glStencilOpSeparate(GL_FRONT, OpenGLUtils::StencilOperationToOpenGL(depth->StencilFrontFailOp),
                                OpenGLUtils::StencilOperationToOpenGL(depth->StencilFrontDepthFailOp),
                                OpenGLUtils::StencilOperationToOpenGL(depth->StencilFrontPassOp));
            glStencilOpSeparate(GL_BACK, OpenGLUtils::StencilOperationToOpenGL(depth->StencilBackFailOp),
                                OpenGLUtils::StencilOperationToOpenGL(depth->StencilBackDepthFailOp),
                                OpenGLUtils::StencilOperationToOpenGL(depth->StencilBackPassOp));
        }
        else
            glDisable(GL_STENCIL_TEST);

        if (raster->CullMode == CullingMode::CULL_NONE)
            glDisable(GL_CULL_FACE);
        else
        {
            glEnable(GL_CULL_FACE);
            glFrontFace(GL_CCW);
            glCullFace(raster->CullMode == CullingMode::CULL_CLOCKWISE ? GL_FRONT : GL_BACK);
        }
        glPolygonMode(GL_FRONT_AND_BACK, PolygonModeToOpenGL(raster->PolygonDrawMode));
        if (raster->DepthBias != 0.0f || raster->DepthBiasSlope != 0.0f)
        {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(raster->DepthBiasSlope, raster->DepthBias);
        }
        else
            glDisable(GL_POLYGON_OFFSET_FILL);
        if (raster->DepthClipEnable)
            glDisable(GL_DEPTH_CLAMP);
        else
            glEnable(GL_DEPTH_CLAMP);
        if (raster->ScissorsEnabled)
            glEnable(GL_SCISSOR_TEST);
        else
            glDisable(GL_SCISSOR_TEST);

        if (blend->EnableBlending)
        {
            glEnable(GL_BLEND);
            glBlendFuncSeparate(OpenGLUtils::BlendFactorToOpenGL(blend->SrcBlend), OpenGLUtils::BlendFactorToOpenGL(blend->DstBlend),
                                OpenGLUtils::BlendFactorToOpenGL(blend->SrcBlendAlpha),
                                OpenGLUtils::BlendFactorToOpenGL(blend->DstBlendAlpha));
            glBlendEquationSeparate(OpenGLUtils::BlendFunctionToOpenGL(blend->BlendOp), OpenGLUtils::BlendFunctionToOpenGL(blend->BlendOpAlpha));
        }
        else
            glDisable(GL_BLEND);
        if (blend->AlphaToCoverage)
            glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        else
            glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }

    void OpenGLRenderAPI::ApplyViewport()
    {
        uint32_t width = 0;
        uint32_t height = 0;
        if (m_RenderTarget)
        {
            const RenderTargetProperties& properties = m_RenderTarget->GetProperties();
            width = properties.Width;
            height = properties.Height;
        }
        else
        {
            GLint current[4] = {};
            glGetIntegerv(GL_VIEWPORT, current);
            width = static_cast<uint32_t>(std::max(current[2], 1));
            height = static_cast<uint32_t>(std::max(current[3], 1));
        }
        glViewport(static_cast<GLint>(m_Viewport.X * width), static_cast<GLint>(m_Viewport.Y * height),
                   static_cast<GLsizei>(m_Viewport.Width * width), static_cast<GLsizei>(m_Viewport.Height * height));
    }

    void OpenGLRenderAPI::OnShutdown()
    {
        m_RenderTarget = nullptr;
        m_GraphicsPipeline = nullptr;
        m_ComputePipeline = nullptr;
        m_IndexBuffer = nullptr;
        m_VertexBuffers.clear();
        m_VertexLayout = nullptr;
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);
        if (m_VertexArray != 0)
        {
            glDeleteVertexArrays(1, &m_VertexArray);
            m_VertexArray = 0;
        }
        RenderAPI::OnShutdown();
    }
} // namespace Crowny
