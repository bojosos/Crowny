#pragma once

#include "Crowny/RenderAPI/Query.h"

namespace Crowny
{
    class OpenGLTimerQuery : public TimerQuery
    {
    public:
        friend class TimerQuery;
        ~OpenGLTimerQuery() override;

    protected:
        OpenGLTimerQuery();
        void Begin(const Ref<CommandBuffer>& cb = nullptr) override;
        void End(const Ref<CommandBuffer>& cb = nullptr) override;
        float GetTimeMs() override;
        bool IsReady() const override;

    private:
        uint32_t m_Query = 0;
    };

    class OpenGLPipelineQuery : public PipelineQuery
    {
    public:
        friend class PipelineQuery;
        ~OpenGLPipelineQuery() override;

    protected:
        OpenGLPipelineQuery();
        bool IsReady() const override;
        void Begin(const Ref<CommandBuffer>& cb = nullptr) override;
        void End(const Ref<CommandBuffer>& cb = nullptr) override;

    private:
        uint32_t m_Query = 0;
    };

    class OpenGLOcclusionQuery : public OcclusionQuery
    {
    public:
        friend class OcclusionQuery;
        ~OpenGLOcclusionQuery() override;

        uint32_t GetRendererID() const { return m_Query; }
        bool IsBinary() const { return m_Binary; }

    protected:
        explicit OpenGLOcclusionQuery(bool binary);

    private:
        uint32_t m_Query = 0;
        bool m_Binary = false;
    };
} // namespace Crowny
