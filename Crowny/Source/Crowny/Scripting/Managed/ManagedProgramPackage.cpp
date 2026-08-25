#include "cwpch.h"

#include "Crowny/Common/DataStream.h"
#include "Crowny/Scripting/Managed/ManagedProgramPackage.h"
#include "Crowny/Scripting/Managed/Interop/CrownyManagedAbi.h"

#include <rapidjson/document.h>

namespace Crowny
{
    namespace
    {
        bool ReadString(const rapidjson::Value& object, const char* name, String& output)
        {
            const auto value = object.FindMember(name);
            if (value == object.MemberEnd() || !value->value.IsString())
                return false;
            output.assign(value->value.GetString(), value->value.GetStringLength());
            return true;
        }

        bool IsSafeRelativePath(const Path& path)
        {
            if (path.empty() || path.is_absolute())
                return false;
            return std::none_of(path.begin(), path.end(), [](const Path& part) { return part == ".."; });
        }

        bool IsInsidePackage(const Path& packageRoot, const Path& resolved)
        {
            std::error_code error;
            const Path relative = fs::relative(resolved, packageRoot, error);
            if (error)
                return false;
            return relative == "." || IsSafeRelativePath(relative);
        }

        ManagedOperationResult ResolveArtifact(const Path& packageRoot, const rapidjson::Value& artifacts, const char* property,
                                               ManagedProgramArtifactKind kind, String logicalName,
                                               Vector<ManagedProgramArtifact>& output)
        {
            String text;
            if (!ReadString(artifacts, property, text))
                return ManagedOperationResult::Failure("managed.package.artifact_missing",
                                                       String("Managed package has no ") + property + " artifact.", ManagedBackendId::CoreCLR);
            const Path relative = Path(text);
            if (!IsSafeRelativePath(relative))
                return ManagedOperationResult::Failure("managed.package.artifact_path_invalid",
                                                       String("Managed package artifact ") + property + " has an unsafe path.",
                                                       ManagedBackendId::CoreCLR);
            std::error_code error;
            const Path resolved = fs::weakly_canonical(packageRoot / relative, error);
            if (error || !IsInsidePackage(packageRoot, resolved) || !fs::is_regular_file(resolved, error) || error)
                return ManagedOperationResult::Failure("managed.package.artifact_not_found",
                                                       String("Managed package artifact ") + property + " does not exist.",
                                                       ManagedBackendId::CoreCLR);
            output.push_back({ kind, std::move(logicalName), resolved });
            return ManagedOperationResult::Success();
        }
    } // namespace

    ManagedProgramPackageResult LoadManagedProgramPackage(const Path& manifestPath, uint64_t generation)
    {
        std::error_code error;
        if (!fs::is_regular_file(manifestPath, error) || error)
            return { ManagedOperationResult::Failure("managed.package.manifest_missing", "Managed package manifest does not exist.",
                                                     ManagedBackendId::CoreCLR),
                     {} };

        FileDataStream stream(manifestPath, DataStream::READ);
        const String json = stream.GetAsString();
        rapidjson::Document document;
        document.Parse(json.data(), json.size());
        if (document.HasParseError() || !document.IsObject() || !document.HasMember("schemaVersion") ||
            !document["schemaVersion"].IsUint() || document["schemaVersion"].GetUint() != 1 || !document.HasMember("abiVersion") ||
            !document["abiVersion"].IsUint() || document["abiVersion"].GetUint() != CW_MANAGED_ABI_VERSION ||
            !document.HasMember("artifacts") || !document["artifacts"].IsObject())
            return { ManagedOperationResult::Failure("managed.package.manifest_invalid",
                                                     "Managed package manifest is invalid or incompatible.", ManagedBackendId::CoreCLR),
                     {} };

        String backend;
        String runtimeRoot;
        if (!ReadString(document, "backend", backend) || backend != "CoreCLR" || !ReadString(document, "runtimeRoot", runtimeRoot))
            return { ManagedOperationResult::Failure("managed.package.backend_invalid",
                                                     "Managed package does not describe a CoreCLR runtime.", ManagedBackendId::CoreCLR),
                     {} };

        const Path absoluteManifest = fs::absolute(manifestPath, error);
        const Path packageRoot = error ? Path() : fs::weakly_canonical(absoluteManifest.parent_path(), error);
        if (error || packageRoot.empty())
            return { ManagedOperationResult::Failure("managed.package.manifest_path_invalid",
                                                     "Managed package manifest path cannot be resolved.", ManagedBackendId::CoreCLR),
                     {} };
        const Path relativeRuntimeRoot(runtimeRoot);
        if (!IsSafeRelativePath(relativeRuntimeRoot))
            return { ManagedOperationResult::Failure("managed.package.runtime_path_invalid",
                                                     "Managed package runtime root has an unsafe path.", ManagedBackendId::CoreCLR),
                     {} };
        const Path resolvedRuntimeRoot = fs::weakly_canonical(packageRoot / relativeRuntimeRoot, error);
        if (error || !IsInsidePackage(packageRoot, resolvedRuntimeRoot) || !fs::is_directory(resolvedRuntimeRoot, error) || error)
            return { ManagedOperationResult::Failure("managed.package.runtime_missing",
                                                     "Managed package private runtime root does not exist.", ManagedBackendId::CoreCLR),
                     {} };

        ManagedProgramPackage package;
        package.Runtime.Backend = ManagedBackendId::CoreCLR;
        package.Runtime.ExecutionMode = ManagedExecutionMode::Jit;
        package.Runtime.RuntimeRoot = resolvedRuntimeRoot;
        package.Program.Generation = generation;

        const rapidjson::Value& artifacts = document["artifacts"];
        struct ArtifactSpec
        {
            const char* Property;
            ManagedProgramArtifactKind Kind;
            const char* LogicalName;
        };
        const ArtifactSpec specs[] = {
            { "nethost", ManagedProgramArtifactKind::NativeLibrary, "nethost" },
            { "hostAssembly", ManagedProgramArtifactKind::EngineAssembly, "managed-host" },
            { "hostDependencies", ManagedProgramArtifactKind::DependencyManifest, "managed-host" },
            { "runtimeConfig", ManagedProgramArtifactKind::RuntimeConfig, "managed-host" },
            { "gameAssembly", ManagedProgramArtifactKind::GameAssembly, "game" },
            { "gameDependencies", ManagedProgramArtifactKind::DependencyManifest, "game" },
        };
        for (const ArtifactSpec& spec : specs)
        {
            ManagedOperationResult resolved =
              ResolveArtifact(packageRoot, artifacts, spec.Property, spec.Kind, spec.LogicalName, package.Program.Artifacts);
            if (!resolved.Succeeded)
                return { std::move(resolved), {} };
        }
        return { ManagedOperationResult::Success(), std::move(package) };
    }
} // namespace Crowny
