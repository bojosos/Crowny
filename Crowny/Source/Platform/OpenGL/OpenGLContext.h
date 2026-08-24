#pragma once

#include "Crowny/RenderAPI/GraphicsContext.h"

namespace Crowny
{

    class OpenGLContext : public GraphicsContext
    {
    public:
        OpenGLContext(void* window);

        virtual void Init() override;
        virtual void SwapBuffers() override;

        void MakeCurrent() const;
        void SetSwapInterval(int interval) const;
        void* GetWindow() const { return m_Window; }

    private:
        void* m_Window = nullptr;
    };

} // namespace Crowny
