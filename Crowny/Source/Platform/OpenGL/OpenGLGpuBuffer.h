#pragma once

#include "Crowny/RenderAPI/GenericGpuBuffer.h"

namespace Crowny
{
    class OpenGLGpuBuffer : public GpuBuffer
    {
    public:
        OpenGLGpuBuffer(uint32_t target, uint32_t size, BufferUsage usage);
        ~OpenGLGpuBuffer() override;

        void* Map(uint32_t offset, uint32_t length, GpuLockOptions options, uint32_t queueIdx = 0) override;
        void Unmap() override;
        void WriteData(uint32_t offset, uint32_t length, const void* src, BufferWriteOptions writeOptions = BWT_NORMAL) override;
        void ReadData(uint32_t offset, uint32_t length, void* dest) override;

        uint32_t GetRendererID() const { return m_RendererID; }
        uint32_t GetTarget() const { return m_Target; }

    private:
        uint32_t m_RendererID = 0;
        uint32_t m_Target = 0;
    };

    class OpenGLGenericGpuBuffer : public GenericGpuBuffer
    {
    public:
        friend class GenericGpuBuffer;
        ~OpenGLGenericGpuBuffer() override = default;

        void* Map(uint32_t offset, uint32_t size, GpuLockOptions options) override;
        void Unmap() override;
        void WriteData(uint32_t offset, uint32_t length, const void* src, BufferWriteOptions writeOptions = BWT_NORMAL) override;
        void ReadData(uint32_t offset, uint32_t length, void* dest) override;
        uint32_t GetBufferSize() const override { return m_Size; }

        uint32_t GetRendererID() const { return m_Buffer.GetRendererID(); }

    protected:
        OpenGLGenericGpuBuffer(uint32_t elementCount, uint32_t elementSize, GpuBufferType type, GpuBufferFormat format, BufferUsage usage);

    private:
        OpenGLGpuBuffer m_Buffer;
        uint32_t m_Size = 0;
        GpuBufferType m_Type = GpuBufferType::Standard;
        GpuBufferFormat m_Format = BF_UNKNOWN;
    };
} // namespace Crowny
