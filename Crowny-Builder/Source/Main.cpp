#include "Crowny/Build/BuilderCli.h"

#include <csignal>
#include <iostream>

namespace
{
    volatile std::sig_atomic_t cancelled = 0;

    void HandleSignal(int) { cancelled = 1; }
} // namespace

int main(int argc, char** argv)
{
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    Crowny::Vector<Crowny::String> arguments;
    arguments.reserve(argc > 1 ? static_cast<size_t>(argc - 1) : 0);
    for (int index = 1; index < argc; index++)
        arguments.emplace_back(argv[index]);

    return Crowny::RunCrownyBuilder(arguments, std::cout, std::cerr, [] { return cancelled != 0; });
}
