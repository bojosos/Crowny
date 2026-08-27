#pragma once

#include "Crowny/Build/BuildTypes.h"
#include "Crowny/Common/StdHeaders.h"

#include <chrono>

namespace Crowny
{
    struct ManagedBuildDiagnostic
    {
        String Code;
        String Message;
        Path Subject;
    };

    struct ManagedAssemblyIdentity
    {
        String Name;
        uint16_t Major = 0;
        uint16_t Minor = 0;
        uint16_t Build = 0;
        uint16_t Revision = 0;
        String Culture;
        String PublicKeyToken;

        bool operator==(const ManagedAssemblyIdentity&) const = default;
        String ToString() const;
    };

    struct ManagedAssemblyInspection
    {
        Path File;
        ManagedAssemblyIdentity Identity;
        Vector<ManagedAssemblyIdentity> References;
        Vector<ManagedBuildDiagnostic> Diagnostics;
        bool IsILOnly = false;
        bool HasPInvoke = false;

        bool IsPureManaged() const { return Diagnostics.empty() && IsILOnly && !HasPInvoke; }
    };

    ManagedAssemblyInspection InspectManagedAssembly(const Path& assembly);

    struct ManagedDependencyRequest
    {
        Vector<Path> Roots;
        Vector<Path> SearchDirectories;
        Vector<Path> FrameworkDirectories;
    };

    struct ManagedDependencyResult
    {
        Vector<Path> Assemblies;
        Vector<ManagedBuildDiagnostic> Diagnostics;

        bool Succeeded() const { return Diagnostics.empty(); }
    };

    ManagedDependencyResult ResolveManagedDependencyClosure(const ManagedDependencyRequest& request);

    struct ManagedToolchain
    {
        Path Root;
        Path RuntimeExecutable;
        Path CompilerAssembly;
        Path ReferenceDirectory;
        String Version;
        Vector<ManagedBuildDiagnostic> Diagnostics;

        bool IsValid() const { return Diagnostics.empty(); }
    };

    ManagedToolchain LocateManagedToolchain(const Path& root);

    struct ManagedBuildRequest
    {
        Path ProjectRoot;
        Path OutputAssembly;
        Vector<Path> Sources;
        Vector<Path> References;
        Vector<String> Symbols;
        BuildConfiguration Configuration = BuildConfiguration::Development;
        String LanguageVersion = "9.0";
        std::chrono::milliseconds Timeout = std::chrono::minutes(2);
        size_t MaxCapturedOutputBytes = 1024 * 1024;
        std::function<bool()> Cancellation;
    };

    struct ManagedBuildPlan
    {
        Vector<String> CompilerArguments;
        String CacheKey;
        Vector<ManagedBuildDiagnostic> Diagnostics;

        bool IsValid() const { return Diagnostics.empty(); }
    };

    ManagedBuildPlan CreateManagedBuildPlan(const ManagedBuildRequest& request, const ManagedToolchain& toolchain);

    struct ManagedCompileResult
    {
        ManagedBuildPlan Plan;
        Vector<ManagedBuildDiagnostic> Diagnostics;
        String StandardOutput;
        String StandardError;
        int ExitCode = -1;
        bool ProcessStarted = false;
        bool Cancelled = false;

        bool Succeeded() const { return ProcessStarted && !Cancelled && ExitCode == 0 && Diagnostics.empty(); }
    };

    ManagedCompileResult CompileManagedAssembly(const ManagedBuildRequest& request, const ManagedToolchain& toolchain);

    struct DotNetSdk
    {
        Path Executable;
        String Version;
        Vector<ManagedBuildDiagnostic> Diagnostics;

        bool IsValid() const { return Diagnostics.empty(); }
    };

    DotNetSdk LocateDotNetSdk(const Path& root = {});

    struct ManagedSdkBuildRequest
    {
        Path ProjectFile;
        Path OutputDirectory;
        String TargetFramework = "net10.0";
        BuildConfiguration Configuration = BuildConfiguration::Development;
        std::chrono::milliseconds Timeout = std::chrono::minutes(5);
        size_t MaxCapturedOutputBytes = 4 * 1024 * 1024;
        std::function<bool()> Cancellation;
    };

    struct ManagedSdkBuildResult
    {
        Vector<ManagedBuildDiagnostic> Diagnostics;
        String StandardOutput;
        String StandardError;
        int ExitCode = -1;
        bool ProcessStarted = false;
        bool Cancelled = false;

        bool Succeeded() const { return ProcessStarted && !Cancelled && ExitCode == 0 && Diagnostics.empty(); }
    };

    ManagedSdkBuildResult BuildManagedSdkProject(const ManagedSdkBuildRequest& request, const DotNetSdk& sdk);
} // namespace Crowny
