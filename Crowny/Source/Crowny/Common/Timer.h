#pragma once

#include <chrono>

namespace Crowny
{

    class Timer
    {
    public:
        Timer() { m_StartTime = std::chrono::steady_clock::now(); }

        uint64_t ElapsedMicros() const
        {

            const auto endTime = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::time_point_cast<std::chrono::microseconds>(endTime).time_since_epoch() -
                           std::chrono::time_point_cast<std::chrono::microseconds>(m_StartTime).time_since_epoch();
            return elapsed.count();
        }

        float ElapsedMillis() const { return ElapsedMicros() / 1000.0f; }

        float ElapsedSeconds() const { return ElapsedMicros() / 1000000.0f; }

        void Reset() { m_StartTime = std::chrono::steady_clock::now(); }

    private:
        std::chrono::time_point<std::chrono::steady_clock> m_StartTime;
    };

} // namespace Crowny