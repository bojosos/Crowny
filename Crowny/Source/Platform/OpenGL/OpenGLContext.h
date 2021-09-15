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

    private:
        void* m_Window;
    };

} // namespace Crowny