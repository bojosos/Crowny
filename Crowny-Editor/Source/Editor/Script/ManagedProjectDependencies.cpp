#include "cwepch.h"

#include "Editor/Script/ManagedProjectDependencies.h"

#include "Crowny/Common/StringUtils.h"

#include <cctype>

namespace Crowny
{
    namespace
    {
        void AddDiagnostic(Vector<ManagedBuildDiagnostic>& diagnostics, String code, String message, Path subject = {})
        {
            diagnostics.push_back({ std::move(code), std::move(message), std::move(subject) });
        }

        Path NormalizePath(const Path& projectRoot, const Path& path)
        {
            Path resolved = path.is_relative() ? projectRoot / path : path;
            resolved = fs::absolute(resolved).lexically_normal();
            std::error_code error;
            const Path canonical = fs::weakly_canonical(resolved, error);
            return error ? resolved : canonical;
        }

        String ComparablePath(const Path& path)
        {
            String value = path.generic_string();
#ifdef CW_PLATFORM_WIN32
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
#endif
            return value;
        }

        String Lowercase(String value)
        {
            StringUtils::ToLower(value);
            return value;
        }

        bool IsWithin(const Path& directory, const Path& path)
        {
            const Path relative = path.lexically_relative(directory);
            if (relative.empty())
                return ComparablePath(directory) == ComparablePath(path);
            return relative == "." || !relative.generic_string().starts_with("..");
        }

        bool ContainsPath(const Vector<Path>& paths, const Path& path)
        {
            return std::any_of(paths.begin(), paths.end(), [&](const Path& value) { return ComparablePath(value) == ComparablePath(path); });
        }

        bool ContainsName(const Vector<String>& names, const String& name)
        {
            return std::any_of(names.begin(), names.end(), [&](const String& value) { return Lowercase(value) == Lowercase(name); });
        }

        bool IsFrameworkAssembly(const Vector<Path>& frameworkDirectories, const Path& assembly)
        {
            return std::any_of(frameworkDirectories.begin(), frameworkDirectories.end(),
                               [&](const Path& directory) { return IsWithin(directory, assembly); });
        }

        void SortAndUniquePaths(Vector<Path>& paths)
        {
            std::sort(paths.begin(), paths.end(), [](const Path& left, const Path& right) { return ComparablePath(left) < ComparablePath(right); });
            paths.erase(std::unique(paths.begin(), paths.end(),
                                    [](const Path& left, const Path& right) { return ComparablePath(left) == ComparablePath(right); }),
                        paths.end());
        }

        void ValidateStagingNames(ManagedProjectDependencyPlan& plan)
        {
            Map<String, Path> assemblyNames;
            Map<String, Path> filenames;
            for (const ManagedProjectDependency& dependency : plan.Assemblies)
            {
                const String assemblyName = Lowercase(dependency.Name);
                const auto existingAssembly = assemblyNames.find(assemblyName);
                if (existingAssembly != assemblyNames.end() && ComparablePath(existingAssembly->second) != ComparablePath(dependency.Filepath))
                    AddDiagnostic(plan.Diagnostics, "MPD106", "Managed dependency assembly names must be unique.", dependency.Filepath);
                else
                    assemblyNames.insert_or_assign(assemblyName, dependency.Filepath);

                const String filename = Lowercase(dependency.Filepath.filename().string());
                const auto existingFilename = filenames.find(filename);
                if (existingFilename != filenames.end() && ComparablePath(existingFilename->second) != ComparablePath(dependency.Filepath))
                    AddDiagnostic(plan.Diagnostics, "MPD105", "Managed dependency filenames must be unique for runtime staging.",
                                  dependency.Filepath);
                else
                    filenames.insert_or_assign(filename, dependency.Filepath);
            }
            if (!plan.Succeeded())
                plan.Assemblies.clear();
        }
    } // namespace

    Vector<Path> FindDotNetFrameworkReferenceDirectories(const DotNetSdk& sdk, StringView targetFramework)
    {
        Vector<Path> candidates;
        if (!sdk.IsValid() || sdk.Executable.empty() || targetFramework.empty())
            return candidates;

        const Path packsDirectory = sdk.Executable.parent_path() / "packs" / "Microsoft.NETCore.App.Ref";
        std::error_code error;
        if (!fs::is_directory(packsDirectory, error))
            return candidates;
        for (const fs::directory_entry& entry : fs::directory_iterator(packsDirectory, error))
        {
            if (error)
                break;
            if (!entry.is_directory(error))
                continue;
            const Path referenceDirectory = entry.path() / "ref" / String(targetFramework);
            if (fs::is_directory(referenceDirectory, error))
                candidates.push_back(NormalizePath({}, referenceDirectory));
            error.clear();
        }
        std::sort(candidates.begin(), candidates.end(),
                  [](const Path& left, const Path& right) { return ComparablePath(left) > ComparablePath(right); });
        if (candidates.size() > 1)
            candidates.resize(1);
        return candidates;
    }

    ManagedProjectDependencyPlan ResolveManagedProjectDependencies(const ManagedProjectDependencyRequest& request)
    {
        ManagedProjectDependencyPlan plan;
        const Path projectRoot = NormalizePath({}, request.ProjectRoot.empty() ? Path(".") : request.ProjectRoot);

        Vector<Path> excluded;
        excluded.reserve(request.ExcludedAssemblies.size());
        for (const Path& assembly : request.ExcludedAssemblies)
            if (!assembly.empty())
                excluded.push_back(NormalizePath(projectRoot, assembly));
        SortAndUniquePaths(excluded);

        Vector<Path> roots;
        Map<String, ManagedProjectDependency> rootAssemblies;
        for (const Path& declared : request.DeclaredAssemblies)
        {
            if (declared.empty())
            {
                AddDiagnostic(plan.Diagnostics, "MPD100", "A managed assembly dependency path is empty.");
                continue;
            }

            const Path assembly = NormalizePath(projectRoot, declared);
            if (Lowercase(assembly.extension().string()) != ".dll" || !fs::is_regular_file(assembly))
            {
                AddDiagnostic(plan.Diagnostics, "MPD101", "Managed assembly dependencies must be existing .dll files.", assembly);
                continue;
            }
            if (ContainsPath(excluded, assembly))
                continue;

            const ManagedAssemblyInspection inspection = InspectManagedAssembly(assembly);
            if (!inspection.Diagnostics.empty())
            {
                plan.Diagnostics.insert(plan.Diagnostics.end(), inspection.Diagnostics.begin(), inspection.Diagnostics.end());
                continue;
            }
            if (!inspection.IsILOnly || inspection.HasPInvoke)
            {
                AddDiagnostic(plan.Diagnostics, "MPD102", "Managed assembly dependencies must be pure managed IL without P/Invoke.", assembly);
                continue;
            }
            if (ContainsName(request.ReservedAssemblyNames, inspection.Identity.Name))
            {
                AddDiagnostic(plan.Diagnostics, "MPD103",
                              "Managed assembly dependency conflicts with an engine-provided assembly: " + inspection.Identity.Name + ".", assembly);
                continue;
            }

            const String identity = inspection.Identity.ToString();
            const auto existing = rootAssemblies.find(identity);
            if (existing != rootAssemblies.end() && ComparablePath(existing->second.Filepath) != ComparablePath(assembly))
            {
                AddDiagnostic(plan.Diagnostics, "MPD104", "More than one file provides managed assembly " + identity + ".", assembly);
                continue;
            }
            rootAssemblies.insert_or_assign(identity, ManagedProjectDependency{ inspection.Identity.Name, assembly });
            roots.push_back(assembly);
        }
        SortAndUniquePaths(roots);
        if (!plan.Succeeded() || roots.empty())
            return plan;

        if (!request.ResolveClosure)
        {
            for (const Path& root : roots)
            {
                const ManagedAssemblyInspection inspection = InspectManagedAssembly(root);
                plan.Assemblies.push_back({ inspection.Identity.Name, root });
            }
            ValidateStagingNames(plan);
            return plan;
        }

        ManagedDependencyRequest closureRequest;
        closureRequest.Roots = roots;
        closureRequest.SearchDirectories = request.SearchDirectories;
        closureRequest.FrameworkDirectories = request.FrameworkDirectories;
        for (const Path& root : roots)
            closureRequest.SearchDirectories.push_back(root.parent_path());
        SortAndUniquePaths(closureRequest.SearchDirectories);
        SortAndUniquePaths(closureRequest.FrameworkDirectories);

        ManagedDependencyResult closure = ResolveManagedDependencyClosure(closureRequest);
        if (!closure.Succeeded())
        {
            plan.Diagnostics = std::move(closure.Diagnostics);
            return plan;
        }

        Map<String, ManagedProjectDependency> dependencies;
        Map<String, Vector<String>> references;
        for (const Path& assembly : closure.Assemblies)
        {
            if (ContainsPath(excluded, assembly) || IsFrameworkAssembly(closureRequest.FrameworkDirectories, assembly))
                continue;

            const ManagedAssemblyInspection inspection = InspectManagedAssembly(assembly);
            if (!inspection.Diagnostics.empty())
            {
                plan.Diagnostics.insert(plan.Diagnostics.end(), inspection.Diagnostics.begin(), inspection.Diagnostics.end());
                continue;
            }
            if (ContainsName(request.ReservedAssemblyNames, inspection.Identity.Name))
                continue;

            const String identity = inspection.Identity.ToString();
            const auto existing = dependencies.find(identity);
            if (existing != dependencies.end() && ComparablePath(existing->second.Filepath) != ComparablePath(assembly))
            {
                AddDiagnostic(plan.Diagnostics, "MPD104", "More than one file provides managed assembly " + identity + ".", assembly);
                continue;
            }
            dependencies.insert_or_assign(identity, ManagedProjectDependency{ inspection.Identity.Name, assembly });
            for (const ManagedAssemblyIdentity& reference : inspection.References)
                references[identity].push_back(reference.ToString());
        }
        if (!plan.Succeeded())
            return plan;

        Set<String> emitted;
        Set<String> visiting;
        std::function<void(const String&)> visit = [&](const String& identity) {
            if (emitted.contains(identity) || !dependencies.contains(identity))
                return;
            if (!visiting.insert(identity).second)
                return;
            for (const String& reference : references[identity])
                visit(reference);
            visiting.erase(identity);
            emitted.insert(identity);
            plan.Assemblies.push_back(dependencies.at(identity));
        };

        Vector<String> identities;
        identities.reserve(dependencies.size());
        for (const auto& [identity, dependency] : dependencies)
            identities.push_back(identity);
        std::sort(identities.begin(), identities.end());
        for (const String& identity : identities)
            visit(identity);

        ValidateStagingNames(plan);
        return plan;
    }
} // namespace Crowny
