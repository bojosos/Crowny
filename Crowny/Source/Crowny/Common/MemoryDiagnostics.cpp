#include "cwpch.h"

#include "Crowny/Common/MemoryDiagnostics.h"

#include <cstdio>

namespace Crowny
{

    ScopedMemoryLeakCheck::ScopedMemoryLeakCheck()
    {
#if defined(CW_ENABLE_CRT_LEAK_CHECKS)
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        _CrtMemCheckpoint(&m_StartState);
#endif
    }

    ScopedMemoryLeakCheck::~ScopedMemoryLeakCheck() { Finish(); }

    bool ScopedMemoryLeakCheck::Finish()
    {
        if (m_Finished)
            return m_LeaksDetected;
        m_Finished = true;

#if defined(CW_ENABLE_CRT_LEAK_CHECKS)
        _CrtMemState endState{};
        _CrtMemState difference{};
        _CrtMemCheckpoint(&endState);
        if (_CrtMemDifference(&difference, &m_StartState, &endState))
        {
            const size_t leakedBlocks = difference.lCounts[_NORMAL_BLOCK] + difference.lCounts[_CLIENT_BLOCK];
            const size_t leakedBytes = difference.lSizes[_NORMAL_BLOCK] + difference.lSizes[_CLIENT_BLOCK];
            m_LeaksDetected = leakedBlocks > 0;
            if (m_LeaksDetected)
            {
                std::fprintf(stderr, "Crowny memory leak check: %zu bytes remain in %zu blocks.\n", leakedBytes, leakedBlocks);
                _CrtMemDumpAllObjectsSince(&m_StartState);
            }
        }
#endif

        return m_LeaksDetected;
    }

} // namespace Crowny
