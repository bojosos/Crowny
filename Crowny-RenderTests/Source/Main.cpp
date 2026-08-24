#include "RenderTestRunner.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Common/MemoryDiagnostics.h"

#include <iostream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <werapi.h>
#endif

int main(int argc, char** argv)
{
#if defined(_WIN32)
    SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    WerSetFlags(WER_FAULT_REPORTING_NO_UI);
#endif

    if (argc >= 4 && Crowny::String(argv[1]) == "--compare-backends")
    {
        Crowny::Path artifacts = "artifacts/render-tests/backend-diff";
        if (argc >= 6 && Crowny::String(argv[4]) == "--artifacts")
            artifacts = argv[5];
        return Crowny::RenderTests::CompareBackendDirectories(argv[2], argv[3], artifacts);
    }

    Crowny::RenderTests::RunnerOptions options;
    Crowny::String error;
    if (!Crowny::RenderTests::ParseOptions(argc, argv, options, error))
    {
        std::cerr << error << "\n\n";
        Crowny::RenderTests::PrintUsage();
        return 2;
    }
    if (options.ShowHelp)
    {
        Crowny::RenderTests::PrintUsage();
        return 0;
    }

    Crowny::ScopedMemoryLeakCheck memoryLeakCheck;
    Crowny::ApplicationDesc applicationDesc;
    applicationDesc.Name = "Crowny Render Tests";
    applicationDesc.PreferredAPI = options.Backend;
    applicationDesc.WorkingDirectory = Crowny::fs::current_path();
    applicationDesc.BuiltInResourcePackPath = applicationDesc.WorkingDirectory / "Crowny-Editor/Resources/Builtin.cwpack";
    applicationDesc.InternalDirectory = "Crowny-RenderTests/Internal";
    applicationDesc.DeferRuntimeServices = true;
    applicationDesc.Window.Title = "Crowny Render Tests";
    applicationDesc.Window.Width = 64u;
    applicationDesc.Window.Height = 64u;
    applicationDesc.Window.Hidden = true;
    applicationDesc.Window.VSync = false;
    applicationDesc.Window.DepthBuffer = false;
    applicationDesc.Window.AllowResize = false;

    int result = 1;
    try
    {
        Crowny::Application::StartUp(applicationDesc);
        result = Crowny::RenderTests::RunSuite(options);
        Crowny::Application::Shutdown();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Render test process failed: " << exception.what() << '\n';
        Crowny::Application::Shutdown();
        result = 2;
    }

    if (memoryLeakCheck.Finish() && result == 0)
        result = 2;
    return result;
}
