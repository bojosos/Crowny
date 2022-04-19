#pragma once

#include "Crowny/RenderAPI/Query.h"

namespace Crowny
{

	class OpenGLTimerQuery : public TimerQuery
	{
	public:
		OpenGLTimerQuery() = default;
		~OpenGLTimerQuery() = default;
		virtual void Begin(const Ref<CommandBuffer>& cb) override { }
		virtual void End(const Ref<CommandBuffer>& cb) override { }

		virtual float GetTimeMs() override { return 0.0f; }
		virtual bool IsReady() const override { return false; }
	};

	class OpenGLPipelineQuery : public PipelineQuery
	{
	public:
		OpenGLPipelineQuery() = default;
		~OpenGLPipelineQuery() = default;
		
		virtual bool IsReady() const override { return false; }
		virtual void Begin(const Ref<CommandBuffer>& cb = nullptr) override {}
		virtual void End(const Ref<CommandBuffer>& cb = nullptr) override {};
	};

	class OpenGLOcclusionQuery : public OcclusionQuery
	{
	public:
		OpenGLOcclusionQuery(bool binary) { };
		~OpenGLOcclusionQuery() = default;
	};

}