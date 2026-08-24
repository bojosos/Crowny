#pragma once

#if defined(CW_ENABLE_CRT_LEAK_CHECKS)
#include <crtdbg.h>
#endif

namespace Crowny
{

    class ScopedMemoryLeakCheck final
    {
    public:
        ScopedMemoryLeakCheck();
        ~ScopedMemoryLeakCheck();

        ScopedMemoryLeakCheck(const ScopedMemoryLeakCheck&) = delete;
        ScopedMemoryLeakCheck& operator=(const ScopedMemoryLeakCheck&) = delete;

        bool Finish();

    private:
#if defined(CW_ENABLE_CRT_LEAK_CHECKS)
        _CrtMemState m_StartState{};
#endif
        bool m_Finished = false;
        bool m_LeaksDetected = false;
    };

} // namespace Crowny
