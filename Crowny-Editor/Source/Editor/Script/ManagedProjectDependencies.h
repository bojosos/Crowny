#pragma once

#include "Crowny/Build/ManagedBuild.h"

namespace Crowny
{
    // A dependency declared by a project author. The path may be absolute or relative to ProjectRoot.
    struct ManagedProjectDependencyRequest
    {
        Path ProjectRoot;
        Vector<Path> DeclaredAssemblies;
        Vector<Path> SearchDirectories;
        Vector<Path> FrameworkDirectories;
        Vector<Path> ExcludedAssemblies;
        Vector<String> ReservedAssemblyNames;
        bool ResolveClosure = true;
    };

    struct ManagedProjectDependency
    {
        String Name;
        Path Filepath;
    };

    // The same result is consumed by the generated IDE project and the managed build/reload path.
    struct ManagedProjectDependencyPlan
    {
        Vector<ManagedProjectDependency> Assemblies;
        Vector<ManagedBuildDiagnostic> Diagnostics;

        bool Succeeded() const { return Diagnostics.empty(); }
    };

    Vector<Path> FindDotNetFrameworkReferenceDirectories(const DotNetSdk& sdk, StringView targetFramework = "net10.0");
    ManagedProjectDependencyPlan ResolveManagedProjectDependencies(const ManagedProjectDependencyRequest& request);
} // namespace Crowny
