#pragma once

#include "Crowny/RenderAPI/CommandBuffer.h"

namespace Crowny
{

    class TimerQuery
    {
    public:
        virtual ~TimerQuery() = default;

        virtual void Begin(const Ref<CommandBuffer>& cb = nullptr) = 0;
        virtual void End(const Ref<CommandBuffer>& cb = nullptr) = 0;

        virtual float GetTimeMs() = 0;
        virtual bool IsReady() const = 0;

        bool IsActive() const { return m_Active; }
        void SetActive(bool active) { m_Active = active; }

        static Ref<TimerQuery> Create();

    protected:
        bool m_Active;
    };

    class PipelineQuery
    {
    public:
        virtual void Begin(const Ref<CommandBuffer>& cb = nullptr) = 0;
        virtual void End(const Ref<CommandBuffer>& cb = nullptr) = 0;

        bool IsActive() const { return m_Active; }
        void SetActive(bool active) { m_Active = active; }

        virtual bool IsReady() const = 0;

        static Ref<PipelineQuery> Create();

    private:
        bool m_Active;
    };

    class OcclusionQuery
    {
    public:
        static Ref<OcclusionQuery> Create(bool binary);
    };

} // namespace Crowny