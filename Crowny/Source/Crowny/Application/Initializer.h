#pragma once

namespace Crowny
{

    struct ApplicationDesc;

    class Initializer
    {
    public:
        static void Init(const ApplicationDesc& applicationDesc);
        static void Shutdown();
    };
} // namespace Crowny