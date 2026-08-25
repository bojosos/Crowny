#include "cwpch.h"

#include "Crowny/Build/BuildPipeline.h"

#include <cctype>
#include <chrono>
#include <fstream>
#include <random>
#include <stdexcept>

#ifdef CW_PLATFORM_WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace Crowny
{
    namespace
    {
        constexpr Array<BuildPipelineStage, 7> PIPELINE_STAGES = {
            BuildPipelineStage::Validate,    BuildPipelineStage::ResolveContent, BuildPipelineStage::CompileManaged,
            BuildPipelineStage::PackContent, BuildPipelineStage::StageTemplate,  BuildPipelineStage::WriteManifest,
            BuildPipelineStage::Publish,
        };

        String NormalizePathText(const Path& path)
        {
            String result = path.lexically_normal().generic_string();
#ifdef CW_PLATFORM_WIN32
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
#endif
            return result;
        }

        Path NormalizePath(const Path& path)
        {
            std::error_code error;
            const Path absolute = fs::absolute(path, error);
            if (error)
                return path.lexically_normal();
            const Path canonical = fs::weakly_canonical(absolute, error);
            return error ? absolute.lexically_normal() : canonical;
        }

        Path ResolveProjectPath(const Path& projectRoot, const Path& path)
        {
            std::error_code error;
            const Path absolute = fs::absolute(path.is_absolute() ? path : projectRoot / path, error);
            return error ? (path.is_absolute() ? path : projectRoot / path).lexically_normal() : absolute.lexically_normal();
        }

        bool IsWithin(const Path& root, const Path& path)
        {
            const String normalizedRoot = NormalizePathText(NormalizePath(root));
            const String normalizedPath = NormalizePathText(NormalizePath(path));
            if (normalizedRoot == normalizedPath)
                return true;
            const String prefix = normalizedRoot.ends_with('/') ? normalizedRoot : normalizedRoot + '/';
            return normalizedPath.starts_with(prefix);
        }

        bool IsLinkOrReparsePoint(const Path& path)
        {
            std::error_code error;
            if (fs::is_symlink(fs::symlink_status(path, error)))
                return true;
#ifdef CW_PLATFORM_WIN32
            const DWORD attributes = GetFileAttributesW(path.c_str());
            return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
            return false;
#endif
        }

        bool TraversesLinkOrReparsePoint(const Path& path)
        {
            std::error_code error;
            const Path absolute = fs::absolute(path, error).lexically_normal();
            if (error)
                return true;
            Path current;
            for (const Path& component : absolute)
            {
                current /= component;
                if (!fs::exists(current, error))
                {
                    error.clear();
                    continue;
                }
                if (error || IsLinkOrReparsePoint(current))
                    return true;
            }
            return false;
        }

        bool TraversesLinkOrReparsePoint(const Path& root, const Path& path)
        {
            std::error_code error;
            const Path absoluteRoot = fs::absolute(root, error).lexically_normal();
            if (error)
                return true;
            const Path absolutePath = fs::absolute(path, error).lexically_normal();
            if (error)
                return true;
            const Path relative = absolutePath.lexically_relative(absoluteRoot);
            if (relative.empty())
                return IsLinkOrReparsePoint(absoluteRoot);
            if (relative.is_absolute() || *relative.begin() == "..")
                return true;
            Path current = absoluteRoot;
            for (const Path& component : relative)
            {
                current /= component;
                if (!fs::exists(current, error))
                {
                    error.clear();
                    continue;
                }
                if (error || IsLinkOrReparsePoint(current))
                    return true;
            }
            return false;
        }

        void SortIssues(BuildValidation& validation)
        {
            std::sort(validation.Issues.begin(), validation.Issues.end(), [](const BuildIssue& left, const BuildIssue& right) {
                return std::tie(left.Severity, left.Code, left.Subject, left.Message) <
                       std::tie(right.Severity, right.Code, right.Subject, right.Message);
            });
        }

        bool HashesMatch(StringView left, StringView right)
        {
            return !left.empty() && left.size() == right.size() &&
                   std::equal(left.begin(), left.end(), right.begin(),
                              [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
        }

        class FingerprintWriter
        {
        public:
            void Add(StringView value)
            {
                m_Data += std::to_string(value.size());
                m_Data.push_back(':');
                m_Data.append(value);
            }

            void Count(StringView tag, size_t value)
            {
                Add(tag);
                AddNumber(value);
            }
            void Field(StringView tag, StringView value)
            {
                Add(tag);
                Add(value);
            }
            template <class T> void FieldNumber(StringView tag, T value)
            {
                Add(tag);
                AddNumber(value);
            }
            void FieldPath(StringView tag, const Path& path)
            {
                Add(tag);
                AddPath(path);
            }
            void File(StringView tag, const Path& path)
            {
                Add(tag);
                AddFile(path);
            }
            template <class T> void AddNumber(T value) { Add(std::to_string(value)); }
            void AddPath(const Path& path) { Add(path.lexically_normal().generic_string()); }
            void AddFile(const Path& path)
            {
                AddPath(path);
                const String hash = ComputeFileSha256(path);
                Add(hash.empty() ? "<missing-or-unreadable>" : hash);
            }
            String Finish() const { return ComputeBytesSha256(reinterpret_cast<const uint8_t*>(m_Data.data()), m_Data.size()); }

        private:
            String m_Data;
        };

        template <class T> Vector<T> Sorted(Vector<T> values)
        {
            std::sort(values.begin(), values.end());
            return values;
        }

        String CreateFingerprint(const BuildPipelineRequest& request)
        {
            FingerprintWriter output;
            output.Field("format", "crowny-build-pipeline-v2");
            output.FieldNumber("game.schema", request.Game.Schema);
            output.Field("game.product", request.Game.ProductName);
            output.Field("game.artifact", request.Game.ArtifactName);
            output.Field("game.version", request.Game.ProductVersion);
            output.Field("game.company", request.Game.Company);
            output.Field("game.windows_icon", request.Game.WindowsIcon.ToString());
            output.Field("game.linux_icon", request.Game.LinuxIcon.ToString());
            output.FieldNumber("game.window.width", request.Game.Window.Width);
            output.FieldNumber("game.window.height", request.Game.Window.Height);
            output.FieldNumber("game.window.resizable", request.Game.Window.Resizable);
            output.FieldNumber("game.window.fullscreen", request.Game.Window.Fullscreen);
            output.FieldNumber("profile.schema", request.Profile.Schema);
            output.Field("profile.id", request.Profile.Id.ToString());
            output.Field("profile.name", request.Profile.Name);
            output.Count("profile.scenes.count", request.Profile.SceneOrder.size());
            for (const UUID& scene : request.Profile.SceneOrder)
                output.Field("profile.scene", scene.ToString());
            output.Field("profile.startup_scene", request.Profile.StartupScene.ToString());
            output.Count("profile.content_roots.count", request.Profile.ContentRoots.size());
            for (const ContentRoot& root : request.Profile.ContentRoots)
            {
                output.FieldNumber("profile.content_root.kind", static_cast<int>(root.Kind));
                output.FieldPath("profile.content_root.path", root.PathValue);
                output.Field("profile.content_root.asset", root.AssetId.ToString());
            }
            Vector<String> excluded;
            for (const UUID& id : request.Profile.ExcludedAssets)
                excluded.push_back(id.ToString());
            output.Count("profile.excluded.count", excluded.size());
            for (const String& id : Sorted(std::move(excluded)))
                output.Field("profile.excluded", id);
            output.Count("profile.symbols.count", request.Profile.Symbols.size());
            for (const String& symbol : Sorted(request.Profile.Symbols))
                output.Field("profile.symbol", symbol);
            output.Field("profile.default_quality", ToString(request.Profile.DefaultQuality));
            Vector<QualityTier> allowedQuality = request.Profile.AllowedQuality;
            std::sort(allowedQuality.begin(), allowedQuality.end());
            output.Count("profile.allowed_quality.count", allowedQuality.size());
            for (QualityTier quality : allowedQuality)
                output.Field("profile.allowed_quality", ToString(quality));
            output.Field("target.id", request.Target.Id.ToString());
            output.Field("target.platform", ToString(request.Target.Platform));
            output.Field("target.configuration", ToString(request.Target.Configuration));
            output.Field("target.default_quality", ToString(request.Target.DefaultQuality));
            output.Field("target.renderers", ToString(request.Target.Renderers));
            output.Field("target.compatibility", ToString(request.Target.Compatibility));
            output.FieldNumber("target.archive", request.Target.Archive);
            output.FieldNumber("target.include_symbols", request.Target.IncludeSymbols);
            output.Count("target.symbols.count", request.Target.Symbols.size());
            for (const String& symbol : Sorted(request.Target.Symbols))
                output.Field("target.symbol", symbol);

            output.FieldNumber("content.schema", request.Content.Schema);
            Vector<ContentAssetRecord> assets = request.Content.Assets;
            std::sort(assets.begin(), assets.end(),
                      [](const ContentAssetRecord& left, const ContentAssetRecord& right) { return left.Id.ToString() < right.Id.ToString(); });
            output.Count("content.assets.count", assets.size());
            for (const ContentAssetRecord& asset : assets)
            {
                output.Field("content.asset.id", asset.Id.ToString());
                output.FieldPath("content.asset.logical_path", asset.LogicalPath);
                output.FieldPath("content.asset.cooked_path", asset.CookedPath);
                output.Field("content.asset.type", asset.Type);
                output.Field("content.asset.source_hash", asset.SourceHash);
                output.File("content.asset.cooked_file", ResolveProjectPath(request.ProjectRoot, asset.CookedPath));
                Vector<String> dependencies;
                for (const UUID& dependency : asset.Dependencies)
                    dependencies.push_back(dependency.ToString());
                output.Count("content.asset.dependencies.count", dependencies.size());
                for (const String& dependency : Sorted(std::move(dependencies)))
                    output.Field("content.asset.dependency", dependency);
            }

            Vector<Path> managedSources = request.Managed.Sources;
            Vector<Path> managedReferences = request.Managed.References;
            std::sort(managedSources.begin(), managedSources.end(),
                      [](const Path& left, const Path& right) { return NormalizePathText(left) < NormalizePathText(right); });
            std::sort(managedReferences.begin(), managedReferences.end(),
                      [](const Path& left, const Path& right) { return NormalizePathText(left) < NormalizePathText(right); });
            output.Count("managed.sources.count", managedSources.size());
            for (const Path& source : managedSources)
                output.File("managed.source",
                            ResolveProjectPath(request.Managed.ProjectRoot.empty() ? request.ProjectRoot : request.Managed.ProjectRoot, source));
            output.Count("managed.references.count", managedReferences.size());
            for (const Path& reference : managedReferences)
                output.File("managed.reference",
                            ResolveProjectPath(request.Managed.ProjectRoot.empty() ? request.ProjectRoot : request.Managed.ProjectRoot, reference));
            output.Count("managed.symbols.count", request.Managed.Symbols.size());
            for (const String& symbol : Sorted(request.Managed.Symbols))
                output.Field("managed.symbol", symbol);
            output.Field("managed.language_version", request.Managed.LanguageVersion);
            output.Field("managed.configuration", ToString(request.Managed.Configuration));
            output.FieldNumber("managed.timeout_ms", request.Managed.Timeout.count());
            output.FieldNumber("managed.max_captured_output_bytes", request.Managed.MaxCapturedOutputBytes);
            output.Field("toolchain.version", request.Toolchain.Version);
            output.File("toolchain.compiler", request.Toolchain.CompilerAssembly);
            if (!request.Toolchain.RuntimeExecutable.empty())
                output.File("toolchain.runtime", request.Toolchain.RuntimeExecutable);
            else
                output.Field("toolchain.runtime", "<none>");
            Vector<Path> frameworkReferences;
            std::error_code frameworkError;
            if (fs::is_directory(request.Toolchain.ReferenceDirectory, frameworkError))
            {
                for (const fs::directory_entry& entry : fs::directory_iterator(request.Toolchain.ReferenceDirectory, frameworkError))
                {
                    if (frameworkError)
                        break;
                    String extension = entry.path().extension().string();
                    std::transform(extension.begin(), extension.end(), extension.begin(),
                                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
                    if (entry.is_regular_file(frameworkError) && extension == ".dll")
                        frameworkReferences.push_back(entry.path());
                }
            }
            std::sort(frameworkReferences.begin(), frameworkReferences.end(),
                      [](const Path& left, const Path& right) { return NormalizePathText(left) < NormalizePathText(right); });
            if (frameworkError)
                output.Field("toolchain.framework_status", "<framework-enumeration-failed>");
            else
                output.Field("toolchain.framework_status", "ok");
            output.Count("toolchain.framework_references.count", frameworkReferences.size());
            for (const Path& reference : frameworkReferences)
                output.File("toolchain.framework_reference", reference);
            Vector<ManagedBuildDiagnostic> toolchainDiagnostics = request.Toolchain.Diagnostics;
            std::sort(toolchainDiagnostics.begin(), toolchainDiagnostics.end(),
                      [](const ManagedBuildDiagnostic& left, const ManagedBuildDiagnostic& right) {
                          return std::tie(left.Code, left.Message, left.Subject) < std::tie(right.Code, right.Message, right.Subject);
                      });
            output.Count("toolchain.diagnostics.count", toolchainDiagnostics.size());
            for (const ManagedBuildDiagnostic& diagnostic : toolchainDiagnostics)
            {
                output.Field("toolchain.diagnostic.code", diagnostic.Code);
                output.Field("toolchain.diagnostic.message", diagnostic.Message);
                output.FieldPath("toolchain.diagnostic.subject", diagnostic.Subject);
            }

            output.FieldNumber("template.schema", request.Template.Schema);
            output.Field("template.engine_version", request.Template.EngineVersion);
            output.Count("template.compatible_versions.count", request.Template.CompatibleEngineVersions.size());
            for (const String& version : Sorted(request.Template.CompatibleEngineVersions))
                output.Field("template.compatible_version", version);
            output.FieldNumber("template.player_abi", request.Template.PlayerAbi);
            output.FieldNumber("template.content_schema_min", request.Template.ContentSchemaMin);
            output.FieldNumber("template.content_schema_max", request.Template.ContentSchemaMax);
            output.Field("template.platform", ToString(request.Template.Platform));
            output.Field("template.configuration", ToString(request.Template.Configuration));
            Vector<RendererBackend> renderers = request.Template.Renderers;
            std::sort(renderers.begin(), renderers.end());
            output.Count("template.renderers.count", renderers.size());
            for (RendererBackend renderer : renderers)
                output.Field("template.renderer", ToString(renderer));
            Vector<PlayerTemplateFile> templateFiles = request.Template.Files;
            std::sort(templateFiles.begin(), templateFiles.end(), [](const PlayerTemplateFile& left, const PlayerTemplateFile& right) {
                return NormalizePathText(left.RelativePath) < NormalizePathText(right.RelativePath);
            });
            output.Count("template.files.count", templateFiles.size());
            for (const PlayerTemplateFile& file : templateFiles)
            {
                output.FieldPath("template.file.path", file.RelativePath);
                output.Field("template.file.hash", file.Sha256);
                output.FieldNumber("template.file.executable", file.Executable);
                output.File("template.file.payload", ResolveProjectPath(request.TemplateRoot, file.RelativePath));
            }
            output.Field("engine.version", request.EngineVersion);
            output.Field("mono.version", request.MonoVersion);
            return output.Finish();
        }

        Vector<RendererBackend> RequiredRenderers(RendererPolicy policy)
        {
            if (policy == RendererPolicy::VulkanOnly)
                return { RendererBackend::Vulkan };
            if (policy == RendererPolicy::OpenGLOnly)
                return { RendererBackend::OpenGL };
            return { RendererBackend::Vulkan, RendererBackend::OpenGL };
        }

        BuildValidation ValidateRequest(const BuildPipelineRequest& request)
        {
            BuildValidation validation = ValidateBuildProfile(request.Game, request.Profile);
            validation.Append(ValidateContentDatabase(request.Content));
            if (!fs::is_directory(request.ProjectRoot))
                validation.Error("pipeline.project_root.missing", "The project root does not exist.", request.ProjectRoot.string());
            if (request.OutputDirectory.empty() || request.OutputDirectory.filename().empty())
                validation.Error("pipeline.output.invalid", "The output directory must name a directory.", request.OutputDirectory.string());
            if (request.EngineVersion.empty())
                validation.Error("pipeline.engine_version.empty", "The engine version is required.");
            const Path managedRoot = request.Managed.ProjectRoot.empty() ? request.ProjectRoot : request.Managed.ProjectRoot;
            const auto protectRoot = [&](const Path& root, StringView label) {
                if (!root.empty() && (IsWithin(root, request.OutputDirectory) || IsWithin(request.OutputDirectory, root)))
                    validation.Error("pipeline.output.overlap", "The output directory overlaps the " + String(label) + ".",
                                     request.OutputDirectory.string());
            };
            const auto protectInput = [&](const Path& path, StringView label) {
                if (!path.empty() && IsWithin(request.OutputDirectory, path))
                    validation.Error("pipeline.output.contains_input", "The output directory contains the " + String(label) + ".",
                                     NormalizePath(path).string());
            };
            protectRoot(request.ProjectRoot, "project root");
            protectRoot(managedRoot, "managed project root");
            protectRoot(request.TemplateRoot, "player template root");
            for (const ContentAssetRecord& asset : request.Content.Assets)
            {
                const Path cooked = ResolveProjectPath(request.ProjectRoot, asset.CookedPath);
                protectInput(cooked, "cooked content input");
                if (!IsWithin(request.ProjectRoot, cooked) || TraversesLinkOrReparsePoint(request.ProjectRoot, cooked))
                    validation.Error("pipeline.content.path_escape", "A cooked content input escapes the project or traverses a link/reparse point.",
                                     cooked.string());
            }
            for (const Path& source : request.Managed.Sources)
            {
                const Path resolved = ResolveProjectPath(managedRoot, source);
                protectInput(resolved, "managed source");
                if (!IsWithin(managedRoot, resolved) || TraversesLinkOrReparsePoint(managedRoot, resolved))
                    validation.Error("pipeline.managed.source_escape", "A managed source escapes its project or traverses a link/reparse point.",
                                     resolved.string());
            }
            for (const Path& reference : request.Managed.References)
            {
                const Path resolved = ResolveProjectPath(managedRoot, reference);
                protectInput(resolved, "managed reference");
                if (TraversesLinkOrReparsePoint(resolved))
                    validation.Error("pipeline.managed.reference_unsafe", "A managed reference traverses a symbolic link or reparse point.",
                                     resolved.string());
            }
            protectInput(request.Toolchain.CompilerAssembly, "managed compiler");
            protectInput(request.Toolchain.RuntimeExecutable, "managed runtime");
            protectRoot(request.Toolchain.Root, "managed toolchain root");
            protectRoot(request.Toolchain.ReferenceDirectory, "managed framework reference root");
            if (!request.Toolchain.CompilerAssembly.empty() && TraversesLinkOrReparsePoint(request.Toolchain.CompilerAssembly))
                validation.Error("pipeline.toolchain.compiler_unsafe", "The managed compiler traverses a symbolic link or reparse point.",
                                 request.Toolchain.CompilerAssembly.string());
            if (!request.Toolchain.RuntimeExecutable.empty() && TraversesLinkOrReparsePoint(request.Toolchain.RuntimeExecutable))
                validation.Error("pipeline.toolchain.runtime_unsafe", "The managed runtime traverses a symbolic link or reparse point.",
                                 request.Toolchain.RuntimeExecutable.string());
            if (!request.Toolchain.ReferenceDirectory.empty() && TraversesLinkOrReparsePoint(request.Toolchain.ReferenceDirectory))
                validation.Error("pipeline.toolchain.references_unsafe",
                                 "The managed framework reference root traverses a symbolic link or reparse point.",
                                 request.Toolchain.ReferenceDirectory.string());
            for (const PlayerTemplateFile& file : request.Template.Files)
            {
                const Path resolved = ResolveProjectPath(request.TemplateRoot, file.RelativePath);
                protectInput(resolved, "player template file");
                if (!IsWithin(request.TemplateRoot, resolved) || TraversesLinkOrReparsePoint(request.TemplateRoot, resolved))
                    validation.Error("pipeline.template.path_escape", "A player template file escapes its root or traverses a link/reparse point.",
                                     resolved.string());
            }
            if (TraversesLinkOrReparsePoint(request.OutputDirectory))
                validation.Error("pipeline.output.link", "The output path traverses a symbolic link or reparse point.",
                                 request.OutputDirectory.string());

            const auto selected = std::find_if(request.Profile.Targets.begin(), request.Profile.Targets.end(),
                                               [&](const BuildTarget& target) { return target.Id == request.Target.Id; });
            if (selected == request.Profile.Targets.end())
                validation.Error("pipeline.target.missing", "The selected target is not part of the build profile.", request.Target.Id.ToString());
            else if (selected->Platform != request.Target.Platform || selected->Configuration != request.Target.Configuration ||
                     selected->Renderers != request.Target.Renderers || selected->Compatibility != request.Target.Compatibility ||
                     selected->DefaultQuality != request.Target.DefaultQuality || selected->Symbols != request.Target.Symbols ||
                     selected->Archive != request.Target.Archive || selected->IncludeSymbols != request.Target.IncludeSymbols)
                validation.Error("pipeline.target.stale", "The selected target differs from the profile snapshot.", request.Target.Id.ToString());
            return validation;
        }

        Path UniqueSibling(const Path& path, StringView label)
        {
            static std::atomic<uint64_t> sequence{ 0 };
#ifdef CW_PLATFORM_WIN32
            const uint64_t process = GetCurrentProcessId();
#else
            const uint64_t process = static_cast<uint64_t>(getpid());
#endif
            uint64_t entropy = 0;
            try
            {
                std::random_device random;
                entropy = (static_cast<uint64_t>(random()) << 32) ^ static_cast<uint64_t>(random());
            }
            catch (...)
            {
                entropy = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
            }
            return path.parent_path() / ("." + path.filename().string() + "." + String(label) + "-" + std::to_string(process) + "-" +
                                         std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)) + "-" + std::to_string(entropy));
        }

        bool CreateOwnedSiblingDirectory(const Path& path, StringView label, Path& output, String& message)
        {
            std::error_code error;
            fs::create_directories(path.parent_path(), error);
            if (error)
            {
                message = error.message();
                return false;
            }
            for (size_t attempt = 0; attempt < 64; attempt++)
            {
                const Path candidate = UniqueSibling(path, label);
                error.clear();
                const bool created = fs::create_directory(candidate, error);
                if (error)
                {
                    message = error.message();
                    return false;
                }
                if (!created)
                    continue;
                if (!fs::is_directory(candidate, error) || error || IsLinkOrReparsePoint(candidate))
                {
                    std::error_code cleanupError;
                    fs::remove(candidate, cleanupError);
                    message = "The newly created directory failed its ownership postcondition.";
                    return false;
                }
                output = candidate;
                return true;
            }
            message = "Could not reserve a unique directory after 64 attempts.";
            return false;
        }

        struct ScopedDirectoryRemoval
        {
            ScopedDirectoryRemoval() = default;
            explicit ScopedDirectoryRemoval(Path path) : Directory(std::move(path)), Owned(true) {}
            ~ScopedDirectoryRemoval()
            {
                if (Owned && !Directory.empty())
                {
                    std::error_code error;
                    fs::remove_all(Directory, error);
                }
            }
            void TakeOwnership(Path path)
            {
                Directory = std::move(path);
                Owned = true;
            }
            void Release() { Owned = false; }
            Path Directory;
            bool Owned = false;
        };

        class CancellationCheckException : public std::runtime_error
        {
        public:
            explicit CancellationCheckException(const String& message) : std::runtime_error(message) {}
        };

        BuildManifest CreateManifest(const BuildPipelineRequest& request, const ContentResolveResult& content, bool hasManagedAssembly)
        {
            BuildManifest manifest;
            manifest.ProductName = request.Game.ProductName;
            manifest.ArtifactName = request.Game.ArtifactName;
            manifest.ProductVersion = request.Game.ProductVersion;
            manifest.Company = request.Game.Company;
            manifest.EngineVersion = request.EngineVersion;
            manifest.MonoVersion = request.MonoVersion;
            manifest.Platform = request.Target.Platform;
            manifest.Configuration = request.Target.Configuration;
            manifest.Renderers = request.Target.Renderers;
            manifest.DefaultQuality = request.Target.DefaultQuality;
            manifest.AllowedQuality = request.Profile.AllowedQuality;
            std::sort(manifest.AllowedQuality.begin(), manifest.AllowedQuality.end());
            manifest.AllowedQuality.erase(std::unique(manifest.AllowedQuality.begin(), manifest.AllowedQuality.end()), manifest.AllowedQuality.end());
            manifest.StartupScene = request.Profile.StartupScene;
            Map<UUID, Path> logicalPaths;
            for (const ResolvedContentAsset& asset : content.Assets)
                logicalPaths.emplace(asset.Asset.Id, asset.Asset.LogicalPath);
            for (size_t index = 0; index < request.Profile.SceneOrder.size(); index++)
            {
                const UUID scene = request.Profile.SceneOrder[index];
                const auto found = logicalPaths.find(scene);
                if (found != logicalPaths.end())
                    manifest.Scenes.push_back({ static_cast<uint32_t>(index), scene, found->second });
            }
            manifest.Paths.ContentPack = "Content/main.cwpack";
            if (hasManagedAssembly)
                manifest.Paths.ManagedAssembly = "Managed/Game.dll";
            return manifest;
        }

        BuildValidation ValidateContentPackOutput(const Path& path, const ContentPackDescriptor& expected, const Vector<ContentPackInput>& inputs)
        {
            BuildValidation validation;
            ContentPackReader reader;
            if (const String error = reader.Open(path); !error.empty())
            {
                validation.Error("pipeline.pack.readback_failed", "The produced content pack cannot be opened: " + error, path.string());
                return validation;
            }
            const ContentPackDescriptor actual = reader.GetDescriptor();
            if (actual.PackId != expected.PackId || actual.EngineVersion != expected.EngineVersion || actual.PlayerAbi != expected.PlayerAbi ||
                actual.ContentSchema != expected.ContentSchema || actual.MountPriority != expected.MountPriority)
                validation.Error("pipeline.pack.descriptor_mismatch", "The produced content pack descriptor differs from the build request.",
                                 path.string());
            if (reader.GetEntries().size() != inputs.size())
                validation.Error("pipeline.pack.entry_count", "The produced content pack has an unexpected entry count.", path.string());
            for (const ContentPackInput& input : inputs)
            {
                const std::optional<ContentPackEntry> byId = reader.Find(input.Id);
                const std::optional<ContentPackEntry> byPath = reader.Find(input.LogicalPath);
                if (!byId || NormalizePortableBuildPath(byId->LogicalPath) != NormalizePortableBuildPath(input.LogicalPath))
                    validation.Error("pipeline.pack.entry_path_mismatch", "A packed UUID maps to the wrong logical path.", input.Id.ToString());
                if (!byPath || byPath->Id != input.Id)
                    validation.Error("pipeline.pack.path_identity_mismatch", "A packed logical path maps to the wrong UUID.",
                                     input.LogicalPath.generic_string());
                Vector<uint8_t> bytes;
                if (const String error = reader.Read(input.Id, bytes); !error.empty())
                {
                    validation.Error("pipeline.pack.entry_read_failed", error, input.LogicalPath.generic_string());
                    continue;
                }
                const String sourceHash = ComputeFileSha256(input.SourcePath);
                const String packedHash = ComputeBytesSha256(bytes.data(), bytes.size());
                if (sourceHash.empty() || packedHash.empty() || sourceHash != packedHash)
                    validation.Error("pipeline.pack.entry_mismatch", "A packed entry does not match its cooked source.",
                                     input.LogicalPath.generic_string());
            }
            return validation;
        }

        BuildValidation ValidateManifestOutput(const Path& path, const BuildManifest& expected)
        {
            BuildValidation validation;
            BuildManifest loaded;
            if (const String error = BuildManifestStore::Load(path, loaded); !error.empty())
            {
                validation.Error("pipeline.manifest.readback_failed", "The produced manifest cannot be loaded: " + error, path.string());
                return validation;
            }
            validation.Append(ValidateBuildManifest(loaded));
            bool matches = loaded.Schema == expected.Schema && loaded.PlayerAbi == expected.PlayerAbi &&
                           loaded.ContentSchema == expected.ContentSchema && loaded.ProductName == expected.ProductName &&
                           loaded.ArtifactName == expected.ArtifactName && loaded.ProductVersion == expected.ProductVersion &&
                           loaded.Company == expected.Company && loaded.EngineVersion == expected.EngineVersion &&
                           loaded.MonoVersion == expected.MonoVersion && loaded.Platform == expected.Platform &&
                           loaded.Configuration == expected.Configuration && loaded.Renderers == expected.Renderers &&
                           loaded.DefaultQuality == expected.DefaultQuality && loaded.AllowedQuality == expected.AllowedQuality &&
                           loaded.StartupScene == expected.StartupScene && loaded.Paths.ContentPack == expected.Paths.ContentPack &&
                           loaded.Paths.ManagedAssembly == expected.Paths.ManagedAssembly && loaded.Paths.MonoRoot == expected.Paths.MonoRoot &&
                           loaded.Scenes.size() == expected.Scenes.size();
            for (size_t index = 0; matches && index < loaded.Scenes.size(); index++)
            {
                const BuildManifestScene& actualScene = loaded.Scenes[index];
                const BuildManifestScene& expectedScene = expected.Scenes[index];
                matches = actualScene.Order == expectedScene.Order && actualScene.Id == expectedScene.Id &&
                          actualScene.LogicalPath == expectedScene.LogicalPath;
            }
            if (!matches)
                validation.Error("pipeline.manifest.readback_mismatch", "The produced manifest differs from the build snapshot.", path.string());
            return validation;
        }

        BuildValidation ValidateTemplateFilesAt(const Path& root, const PlayerTemplateManifest& manifest)
        {
            BuildValidation validation;
            for (const PlayerTemplateFile& file : manifest.Files)
            {
                const Path staged = root / file.RelativePath;
                if (!IsSafeRelativeBuildPath(file.RelativePath) || !IsWithin(root, staged) || TraversesLinkOrReparsePoint(root, staged) ||
                    !fs::is_regular_file(staged))
                {
                    validation.Error("pipeline.template.output_unsafe",
                                     "A template file is missing, escapes the output, or traverses a link/reparse point.",
                                     file.RelativePath.generic_string());
                    continue;
                }
                const String hash = ComputeFileSha256(staged);
                if (!HashesMatch(hash, file.Sha256))
                    validation.Error("pipeline.template.output_mismatch", "A staged template file does not match its manifest.",
                                     file.RelativePath.generic_string());
            }
            return validation;
        }

        BuildValidation ValidateGeneratedArtifactsAt(const Path& root, const Vector<BuildPipelineArtifact>& artifacts)
        {
            BuildValidation validation;
            for (const BuildPipelineArtifact& artifact : artifacts)
            {
                const Path destination = root / artifact.RelativeDestination;
                if (!IsSafeRelativeBuildPath(artifact.RelativeDestination) || !IsWithin(root, destination) ||
                    TraversesLinkOrReparsePoint(root, destination) || !fs::is_regular_file(destination))
                {
                    validation.Error("pipeline.artifact.output_unsafe",
                                     "A generated artifact is missing, escapes the output, or traverses a link/reparse point.",
                                     artifact.RelativeDestination.generic_string());
                    continue;
                }
                const String sourceHash = ComputeFileSha256(artifact.Source);
                const String destinationHash = ComputeFileSha256(destination);
                if (sourceHash.empty() || destinationHash.empty() || sourceHash != destinationHash)
                    validation.Error("pipeline.artifact.output_mismatch", "A published artifact does not match its generated source.",
                                     artifact.RelativeDestination.generic_string());
            }
            return validation;
        }

        BuildValidation ValidateDirectoryTree(const Path& root, StringView code)
        {
            BuildValidation validation;
            std::error_code error;
            if (!fs::is_directory(root, error) || error || IsLinkOrReparsePoint(root))
            {
                validation.Error(String(code), "The directory is missing or is a symbolic link/reparse point.", root.string());
                return validation;
            }
            for (fs::recursive_directory_iterator iterator(root, fs::directory_options::none, error), end; !error && iterator != end;
                 iterator.increment(error))
            {
                if (IsLinkOrReparsePoint(iterator->path()) || !IsWithin(root, iterator->path()))
                {
                    validation.Error(String(code), "The directory tree contains an escaping symbolic link or reparse point.",
                                     iterator->path().string());
                    iterator.disable_recursion_pending();
                }
            }
            if (error)
                validation.Error(String(code), "The directory tree cannot be inspected: " + error.message(), root.string());
            return validation;
        }

        BuildValidation CopyArtifacts(const BuildTemplateStageRequest& request)
        {
            BuildValidation validation = ValidatePlayerTemplate(request.TemplateRoot, request.Template, request.Validation);
            if (!validation.IsValid())
                return validation;
            if (const String error = StagePlayerTemplate(request.TemplateRoot, request.Template, request.StageDirectory); !error.empty())
            {
                validation.Error("pipeline.template.stage_failed", error, request.StageDirectory.string());
                return validation;
            }
            if (!fs::is_directory(request.StageDirectory) || TraversesLinkOrReparsePoint(request.StageDirectory))
            {
                validation.Error("pipeline.template.stage_unsafe", "The staged player is missing or traverses a symbolic link/reparse point.",
                                 request.StageDirectory.string());
                return validation;
            }
            Vector<BuildPipelineArtifact> artifacts = request.Artifacts;
            std::sort(artifacts.begin(), artifacts.end(), [](const BuildPipelineArtifact& left, const BuildPipelineArtifact& right) {
                return NormalizePathText(left.RelativeDestination) < NormalizePathText(right.RelativeDestination);
            });
            Set<String> destinations;
            Set<String> foldedDestinations;
            for (const BuildPipelineArtifact& artifact : artifacts)
            {
                const String relative = NormalizePathText(artifact.RelativeDestination);
                String folded = relative;
                std::transform(folded.begin(), folded.end(), folded.begin(),
                               [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
                const bool caseCollision = request.Validation.Platform == BuildPlatform::WindowsX64 && !foldedDestinations.insert(folded).second;
                if (!IsSafeRelativeBuildPath(artifact.RelativeDestination) || !destinations.insert(relative).second || caseCollision)
                {
                    validation.Error("pipeline.artifact.path_invalid",
                                     "A generated artifact path is unsafe, duplicated, or collides on the target filesystem.", relative);
                    continue;
                }
                if (!fs::is_regular_file(artifact.Source) || TraversesLinkOrReparsePoint(artifact.Source))
                {
                    validation.Error("pipeline.artifact.missing", "A generated artifact is missing or traverses a symbolic link/reparse point.",
                                     artifact.Source.string());
                    continue;
                }
                const Path destination = request.StageDirectory / artifact.RelativeDestination;
                if (!IsWithin(request.StageDirectory, destination) || TraversesLinkOrReparsePoint(request.StageDirectory, destination.parent_path()))
                {
                    validation.Error("pipeline.artifact.path_invalid", "A generated artifact resolves outside the staged player.", relative);
                    continue;
                }
                std::error_code error;
                fs::create_directories(destination.parent_path(), error);
                if (!error)
                    fs::copy_file(artifact.Source, destination, fs::copy_options::none, error);
                if (error)
                {
                    validation.Error("pipeline.artifact.copy_failed", "Cannot stage generated artifact: " + error.message(), relative);
                    continue;
                }
                const String sourceHash = ComputeFileSha256(artifact.Source);
                const String destinationHash = ComputeFileSha256(destination);
                if (sourceHash.empty() || destinationHash.empty() || sourceHash != destinationHash)
                    validation.Error("pipeline.artifact.hash_mismatch", "A staged artifact does not match its source.", relative);
            }
            return validation;
        }

        String RenameDirectory(const Path& source, const Path& destination)
        {
            std::error_code error;
            fs::rename(source, destination, error);
            return error ? error.message() : String();
        }

        struct MoveAttempt
        {
            String Error;
        };

        MoveAttempt TryMoveDirectory(const BuildPipelineOperations& operations, const Path& source, const Path& destination)
        {
            try
            {
                return { operations.MoveDirectory(source, destination) };
            }
            catch (const std::exception& exception)
            {
                return { "Directory move threw an exception: " + String(exception.what()) };
            }
            catch (...)
            {
                return { "Directory move threw an unknown exception." };
            }
        }

        bool DirectoryExists(const Path& path)
        {
            std::error_code error;
            return fs::is_directory(path, error) && !error;
        }

        bool PathExists(const Path& path)
        {
            std::error_code error;
            return fs::exists(path, error) && !error;
        }

        Path AvailableSibling(const Path& output, StringView label)
        {
            for (size_t attempt = 0; attempt < 64; attempt++)
            {
                const Path candidate = UniqueSibling(output, label);
                std::error_code error;
                const fs::file_status status = fs::symlink_status(candidate, error);
                if (!error && status.type() == fs::file_type::not_found)
                    return candidate;
                if (error == std::errc::no_such_file_or_directory)
                    return candidate;
            }
            return {};
        }

        Path PublicationJournalPath(const Path& output) { return output.parent_path() / ("." + output.filename().string() + ".publish-journal"); }

        String WritePublicationJournal(const Path& output, const Path& backup, StringView phase)
        {
            const Path journal = PublicationJournalPath(output);
            if (IsLinkOrReparsePoint(journal))
                return "Publication recovery journal is a symbolic link or reparse point: '" + journal.string() + "'.";
            std::ofstream stream(journal, std::ios::binary | std::ios::trunc);
            if (!stream)
                return "Cannot create publication recovery journal '" + journal.string() + "'.";
            stream << "phase=" << phase << '\n' << "backup=" << backup.filename().generic_string() << '\n';
            stream.flush();
            return stream ? String() : "Cannot flush publication recovery journal '" + journal.string() + "'.";
        }

        void RemovePublicationJournal(const Path& output, BuildValidation& validation)
        {
            std::error_code error;
            fs::remove(PublicationJournalPath(output), error);
            if (error)
                validation.Warn("pipeline.publish.journal_cleanup_failed", "The publication journal could not be removed: " + error.message(),
                                PublicationJournalPath(output).string());
        }

        struct PublicationJournal
        {
            String Phase;
            Path Backup;
        };

        String ReadPublicationJournal(const Path& output, PublicationJournal& result)
        {
            const Path journal = PublicationJournalPath(output);
            if (IsLinkOrReparsePoint(journal))
                return "The publication journal is a symbolic link or reparse point.";
            std::ifstream stream(journal, std::ios::binary);
            if (!stream)
                return "The publication journal cannot be opened.";
            String phase;
            String backupName;
            String line;
            while (std::getline(stream, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (line.starts_with("phase=") && phase.empty())
                    phase = line.substr(6);
                else if (line.starts_with("backup=") && backupName.empty())
                    backupName = line.substr(7);
                else if (!line.empty())
                    return "The publication journal contains an unknown or duplicate field.";
            }
            if (!stream.eof())
                return "The publication journal cannot be read.";
            if (phase != "prepared" && phase != "backed_up" && phase != "published")
                return "The publication journal has an invalid phase.";
            const String expectedPrefix = "." + output.filename().string() + ".previous-";
            const Path relativeBackup = backupName;
            if (backupName.empty() || !relativeBackup.parent_path().empty() || relativeBackup.filename() != relativeBackup ||
                !backupName.starts_with(expectedPrefix))
                return "The publication journal has an invalid rollback identity.";
            const Path backup = output.parent_path() / relativeBackup;
            if (!IsWithin(output.parent_path(), backup) || IsLinkOrReparsePoint(backup))
                return "The publication journal rollback path is unsafe.";
            result = { std::move(phase), backup };
            return {};
        }

        BuildValidation ReconcilePublication(const Path& output)
        {
            BuildValidation validation;
            Vector<Path> backups;
            std::error_code error;
            if (fs::is_directory(output.parent_path(), error))
            {
                const String prefix = "." + output.filename().string() + ".previous-";
                for (fs::directory_iterator iterator(output.parent_path(), error), end; !error && iterator != end; iterator.increment(error))
                {
                    if (iterator->path().filename().string().starts_with(prefix))
                        backups.push_back(iterator->path());
                }
            }
            if (error)
            {
                validation.Error("pipeline.publish.recovery_scan_failed", "Cannot inspect publication recovery state: " + error.message(),
                                 output.parent_path().string());
                return validation;
            }
            std::sort(backups.begin(), backups.end());
            const Path journalPath = PublicationJournalPath(output);
            const bool journalPresent = PathExists(journalPath) || IsLinkOrReparsePoint(journalPath);
            if (!journalPresent)
            {
                if (!DirectoryExists(output) && !backups.empty())
                    validation.Error("pipeline.publish.recovery_journal_missing",
                                     "Rollback directories exist without a journal; none was restored automatically.", backups.front().string());
                else
                {
                    for (const Path& backup : backups)
                        validation.Warn("pipeline.publish.recovery_preserved",
                                        "A rollback directory without a journal was preserved for manual recovery.", backup.string());
                }
                return validation;
            }

            PublicationJournal journal;
            if (const String journalError = ReadPublicationJournal(output, journal); !journalError.empty())
            {
                validation.Error("pipeline.publish.recovery_journal_invalid", journalError, journalPath.string());
                return validation;
            }
            for (const Path& backup : backups)
            {
                if (NormalizePathText(NormalizePath(backup)) != NormalizePathText(NormalizePath(journal.Backup)))
                    validation.Warn("pipeline.publish.recovery_preserved", "A prior publication backup was preserved for manual recovery.",
                                    backup.string());
            }

            const bool backupPresent = DirectoryExists(journal.Backup) && !IsLinkOrReparsePoint(journal.Backup);
            const bool outputPresent = DirectoryExists(output) && !IsLinkOrReparsePoint(output);
            const bool outputPathExists = PathExists(output);
            if ((PathExists(journal.Backup) && !backupPresent) || (outputPathExists && IsLinkOrReparsePoint(output)))
            {
                validation.Error("pipeline.publish.recovery_state_invalid",
                                 "The journal-identified output or rollback path has an unsafe filesystem type.", journal.Backup.string());
                return validation;
            }
            const auto restoreLastGood = [&](bool quarantineOutput) {
                Path quarantine;
                if (quarantineOutput && PathExists(output))
                {
                    quarantine = AvailableSibling(output, "recovery-candidate");
                    if (quarantine.empty())
                    {
                        validation.Error("pipeline.publish.recovery_failed", "Cannot reserve a quarantine path for the unvalidated output.",
                                         journal.Backup.string());
                        return;
                    }
                    const String quarantineError = RenameDirectory(output, quarantine);
                    if (!quarantineError.empty() || !PathExists(quarantine) || PathExists(output))
                    {
                        validation.Error("pipeline.publish.recovery_failed", "Cannot quarantine the unvalidated output: " + quarantineError,
                                         journal.Backup.string());
                        return;
                    }
                }
                const String restoreError = RenameDirectory(journal.Backup, output);
                if (!restoreError.empty() || !DirectoryExists(output) || PathExists(journal.Backup))
                {
                    validation.Error("pipeline.publish.recovery_failed", "Cannot restore the journal-identified previous build: " + restoreError,
                                     journal.Backup.string());
                    return;
                }
                if (!quarantine.empty())
                {
                    error.clear();
                    fs::remove_all(quarantine, error);
                    if (error)
                        validation.Warn("pipeline.publish.recovery_candidate_cleanup_failed",
                                        "The unvalidated output was quarantined but could not be removed: " + error.message(), quarantine.string());
                }
                RemovePublicationJournal(output, validation);
                validation.Warn("pipeline.publish.recovered", "Recovered the journal-identified previous build after an interrupted publication.",
                                output.string());
            };

            if (journal.Phase == "prepared")
            {
                if (outputPresent && !backupPresent)
                    RemovePublicationJournal(output, validation);
                else if (!outputPresent && backupPresent)
                    restoreLastGood(false);
                else
                    validation.Error("pipeline.publish.recovery_state_invalid",
                                     "The prepared journal does not match the output and rollback directory state.", journal.Backup.string());
                return validation;
            }
            if (journal.Phase == "backed_up")
            {
                if (!backupPresent)
                    validation.Error("pipeline.publish.recovery_backup_missing",
                                     "The journal-identified previous build is missing; no other backup was used.", journal.Backup.string());
                else
                    restoreLastGood(outputPathExists);
                return validation;
            }

            if (outputPresent)
            {
                if (backupPresent)
                {
                    fs::remove_all(journal.Backup, error);
                    if (error)
                        validation.Warn("pipeline.publish.backup_cleanup_failed",
                                        "The validated publication is active, but its old backup could not be removed: " + error.message(),
                                        journal.Backup.string());
                }
                RemovePublicationJournal(output, validation);
            }
            else if (backupPresent)
                restoreLastGood(outputPathExists);
            else
                validation.Error("pipeline.publish.recovery_state_invalid",
                                 "The published journal has neither its validated output nor its previous build.", journal.Backup.string());
            return validation;
        }

        BuildValidation PublishDirectory(const Path& candidate, const Path& output, const BuildPipelineOperations& operations,
                                         const std::function<BuildValidation(const Path&)>& validateOutput)
        {
            BuildValidation validation = ReconcilePublication(output);
            if (!validation.IsValid())
                return validation;
            if (!fs::is_directory(candidate))
            {
                validation.Error("pipeline.publish.candidate_missing", "The staged player directory does not exist.", candidate.string());
                return validation;
            }
            std::error_code error;
            fs::create_directories(output.parent_path(), error);
            if (error)
            {
                validation.Error("pipeline.publish.parent_failed", "Cannot create the output parent directory: " + error.message(),
                                 output.parent_path().string());
                return validation;
            }
            if (!operations.MoveDirectory)
            {
                validation.Error("pipeline.operation.missing", "The directory publication operation is missing.", "Publish");
                return validation;
            }
            const Path backup = AvailableSibling(output, "previous");
            if (backup.empty())
            {
                validation.Error("pipeline.publish.backup_failed", "Cannot reserve a unique rollback path.", output.string());
                return validation;
            }
            error.clear();
            const bool hadPrevious = fs::exists(output, error);
            if (error)
            {
                validation.Error("pipeline.publish.output_inspection_failed", "Cannot inspect the existing output: " + error.message(),
                                 output.string());
                return validation;
            }
            if (hadPrevious && !fs::is_directory(output))
            {
                validation.Error("pipeline.publish.output_not_directory", "The existing output is not a directory.", output.string());
                return validation;
            }
            if (hadPrevious)
            {
                if (IsLinkOrReparsePoint(output))
                {
                    validation.Error("pipeline.publish.output_unsafe", "The existing output is a symbolic link or reparse point.", output.string());
                    return validation;
                }
                if (const String journalError = WritePublicationJournal(output, backup, "prepared"); !journalError.empty())
                {
                    validation.Error("pipeline.publish.journal_failed", journalError, PublicationJournalPath(output).string());
                    return validation;
                }
                const MoveAttempt backupMove = TryMoveDirectory(operations, output, backup);
                const bool backupPresent = DirectoryExists(backup);
                const bool outputPresent = PathExists(output);
                if (!backupMove.Error.empty() || !backupPresent || outputPresent)
                {
                    String message = backupMove.Error.empty() ? "The backup move violated its postconditions." : backupMove.Error;
                    if (backupPresent && !outputPresent)
                    {
                        const MoveAttempt restore = TryMoveDirectory(operations, backup, output);
                        if (!DirectoryExists(output) || DirectoryExists(backup))
                            validation.Error("pipeline.publish.rollback_failed",
                                             "The previous build was moved but could not be restored: " + restore.Error, backup.string());
                        else
                            RemovePublicationJournal(output, validation);
                    }
                    else if (backupPresent)
                        validation.Error("pipeline.publish.rollback_failed",
                                         "Both output and rollback directories exist; the rollback copy was preserved.", backup.string());
                    else if (outputPresent)
                        RemovePublicationJournal(output, validation);
                    else
                        validation.Error("pipeline.publish.rollback_failed",
                                         "Neither the output nor rollback directory exists after the backup operation.", backup.string());
                    validation.Error("pipeline.publish.backup_failed", "Cannot preserve the previous build: " + message, output.string());
                    return validation;
                }
                if (const String journalError = WritePublicationJournal(output, backup, "backed_up"); !journalError.empty())
                {
                    const MoveAttempt restore = TryMoveDirectory(operations, backup, output);
                    if (!DirectoryExists(output) || DirectoryExists(backup))
                        validation.Error("pipeline.publish.rollback_failed",
                                         journalError + " The previous build remains at the reported backup path. " + restore.Error, backup.string());
                    else
                        RemovePublicationJournal(output, validation);
                    validation.Error("pipeline.publish.journal_failed", journalError, PublicationJournalPath(output).string());
                    return validation;
                }
            }
            const MoveAttempt publishMove = TryMoveDirectory(operations, candidate, output);
            BuildValidation outputValidation;
            if (publishMove.Error.empty() && DirectoryExists(output) && !PathExists(candidate) && !IsLinkOrReparsePoint(output) && validateOutput)
            {
                try
                {
                    outputValidation = validateOutput(output);
                }
                catch (const std::exception& exception)
                {
                    outputValidation.Error("pipeline.publish.validation_exception",
                                           "Published output validation threw an exception: " + String(exception.what()), output.string());
                }
                catch (...)
                {
                    outputValidation.Error("pipeline.publish.validation_exception", "Published output validation threw an unknown exception.",
                                           output.string());
                }
            }
            if (!publishMove.Error.empty() || !DirectoryExists(output) || PathExists(candidate) || IsLinkOrReparsePoint(output) ||
                !outputValidation.IsValid())
            {
                String message = publishMove.Error.empty() ? "The publish operation violated its postconditions." : publishMove.Error;
                validation.Append(outputValidation);
                Path failedOutput;
                if (PathExists(output))
                {
                    failedOutput = AvailableSibling(output, "failed");
                    if (!failedOutput.empty())
                    {
                        const MoveAttempt quarantine = TryMoveDirectory(operations, output, failedOutput);
                        if (!PathExists(failedOutput) || PathExists(output))
                        {
                            message += " The failed output could not be quarantined: " + quarantine.Error;
                            failedOutput.clear();
                        }
                    }
                }
                if (hadPrevious)
                {
                    if (PathExists(output))
                    {
                        validation.Error("pipeline.publish.rollback_failed",
                                         "Publishing failed and the new output blocks rollback. The previous build remains at this path.",
                                         backup.string());
                    }
                    else
                    {
                        const MoveAttempt rollback = TryMoveDirectory(operations, backup, output);
                        if (!DirectoryExists(output) || DirectoryExists(backup))
                            validation.Error("pipeline.publish.rollback_failed",
                                             "Publishing failed and the previous build could not be restored: " + rollback.Error, backup.string());
                        else
                        {
                            RemovePublicationJournal(output, validation);
                            if (!failedOutput.empty())
                            {
                                error.clear();
                                fs::remove_all(failedOutput, error);
                                if (error)
                                    validation.Warn("pipeline.publish.failed_output_cleanup",
                                                    "The failed output was quarantined but could not be removed: " + error.message(),
                                                    failedOutput.string());
                            }
                        }
                    }
                }
                else if (!failedOutput.empty())
                {
                    error.clear();
                    fs::remove_all(failedOutput, error);
                    if (error)
                        validation.Warn("pipeline.publish.failed_output_cleanup",
                                        "The failed output was quarantined but could not be removed: " + error.message(), failedOutput.string());
                }
                validation.Error("pipeline.publish.failed", "Cannot publish the staged player: " + message, output.string());
                return validation;
            }
            validation.Append(outputValidation);
            if (hadPrevious)
            {
                if (const String journalError = WritePublicationJournal(output, backup, "published"); !journalError.empty())
                    validation.Warn("pipeline.publish.journal_failed", journalError, PublicationJournalPath(output).string());
                fs::remove_all(backup, error);
                if (error)
                    validation.Warn("pipeline.publish.backup_cleanup_failed", "The previous build backup could not be removed: " + error.message(),
                                    backup.string());
                RemovePublicationJournal(output, validation);
            }
            return validation;
        }

        BuildPipelineStageReport& StageReport(BuildPipelineReport& report, BuildPipelineStage stage)
        {
            return *std::find_if(report.Stages.begin(), report.Stages.end(),
                                 [&](const BuildPipelineStageReport& entry) { return entry.Stage == stage; });
        }

        void SkipAfter(BuildPipelineReport& report, BuildPipelineStage stage)
        {
            bool found = false;
            for (BuildPipelineStageReport& entry : report.Stages)
            {
                if (entry.Stage == stage)
                {
                    found = true;
                    continue;
                }
                if (found && entry.Status == BuildPipelineStageStatus::Pending)
                    entry.Status = BuildPipelineStageStatus::Skipped;
            }
        }
    } // namespace

    const char* ToString(BuildPipelineStage stage)
    {
        switch (stage)
        {
        case BuildPipelineStage::Validate:
            return "Validate";
        case BuildPipelineStage::ResolveContent:
            return "Resolve Content";
        case BuildPipelineStage::CompileManaged:
            return "Compile Managed";
        case BuildPipelineStage::PackContent:
            return "Pack";
        case BuildPipelineStage::StageTemplate:
            return "Stage Template";
        case BuildPipelineStage::WriteManifest:
            return "Write Manifest";
        case BuildPipelineStage::Publish:
            return "Publish";
        }
        return "Unknown";
    }

    const char* ToString(BuildPipelineStageStatus status)
    {
        switch (status)
        {
        case BuildPipelineStageStatus::Pending:
            return "Pending";
        case BuildPipelineStageStatus::Succeeded:
            return "Succeeded";
        case BuildPipelineStageStatus::Failed:
            return "Failed";
        case BuildPipelineStageStatus::Cancelled:
            return "Cancelled";
        case BuildPipelineStageStatus::Skipped:
            return "Skipped";
        }
        return "Pending";
    }

    bool BuildPipelineReport::Succeeded() const
    {
        if (Cancelled || Stages.empty())
            return false;
        return std::all_of(Stages.begin(), Stages.end(), [](const BuildPipelineStageReport& stage) {
            return (stage.Status == BuildPipelineStageStatus::Succeeded || stage.Status == BuildPipelineStageStatus::Skipped) &&
                   stage.Diagnostics.IsValid();
        });
    }

    const BuildPipelineStageReport* BuildPipelineReport::Find(BuildPipelineStage stage) const
    {
        const auto found = std::find_if(Stages.begin(), Stages.end(), [&](const BuildPipelineStageReport& entry) { return entry.Stage == stage; });
        return found == Stages.end() ? nullptr : &*found;
    }

    BuildPipelineOperations CreateDefaultBuildPipelineOperations()
    {
        BuildPipelineOperations operations;
        operations.Validate = [](const BuildPipelineRequest&) { return BuildValidation(); };
        operations.ResolveContent = [](const ContentDatabase& database, const ContentResolveRequest& request) {
            return Crowny::ResolveContent(database, request);
        };
        operations.CompileManaged = [](const ManagedBuildRequest& request, const ManagedToolchain& toolchain) {
            return CompileManagedAssembly(request, toolchain);
        };
        operations.PackContent = [](const Path& path, const ContentPackDescriptor& descriptor, const Vector<ContentPackInput>& inputs) {
            return ContentPackWriter::Write(path, descriptor, inputs);
        };
        operations.StageTemplate = CopyArtifacts;
        operations.WriteManifest = [](const Path& path, const BuildManifest& manifest) { return BuildManifestStore::Save(path, manifest); };
        operations.MoveDirectory = RenameDirectory;
        operations.ValidateProducedArtifacts = true;
        return operations;
    }

    BuildPipeline::BuildPipeline() : m_Operations(CreateDefaultBuildPipelineOperations()) {}

    BuildPipeline::BuildPipeline(BuildPipelineOperations operations) : m_Operations(std::move(operations)) {}

    BuildPipelineReport BuildPipeline::Run(BuildPipelineRequest request, BuildCancellationCheck cancellation) const
    {
        const BuildPipelineRequest snapshot = std::move(request);
        BuildPipelineReport report;
        for (BuildPipelineStage stage : PIPELINE_STAGES)
            report.Stages.push_back({ stage });
        BuildPipelineStage activeStage = BuildPipelineStage::Validate;
        try
        {
            report.Fingerprint = CreateFingerprint(snapshot);
            report.OutputDirectory = NormalizePath(snapshot.OutputDirectory);

            const auto cancelled = [&]() {
                try
                {
                    return cancellation && cancellation();
                }
                catch (const std::exception& exception)
                {
                    throw CancellationCheckException(exception.what());
                }
                catch (...)
                {
                    throw CancellationCheckException("unknown exception");
                }
            };
            const auto cancelAt = [&](BuildPipelineStage stage) {
                BuildPipelineStageReport& current = StageReport(report, stage);
                current.Status = BuildPipelineStageStatus::Cancelled;
                current.Diagnostics.Error("pipeline.cancelled", "The build was cancelled before " + String(ToString(stage)) + ".", ToString(stage));
                report.Cancelled = true;
                SkipAfter(report, stage);
            };
            const auto finishStage = [&](BuildPipelineStage stage, BuildValidation validation) {
                SortIssues(validation);
                BuildPipelineStageReport& current = StageReport(report, stage);
                current.Diagnostics = std::move(validation);
                current.Status = current.Diagnostics.IsValid() ? BuildPipelineStageStatus::Succeeded : BuildPipelineStageStatus::Failed;
                if (current.Status == BuildPipelineStageStatus::Failed)
                    SkipAfter(report, stage);
                return current.Status == BuildPipelineStageStatus::Succeeded;
            };

            activeStage = BuildPipelineStage::Validate;
            if (cancelled())
            {
                cancelAt(BuildPipelineStage::Validate);
                return report;
            }
            BuildValidation validation = ValidateRequest(snapshot);
            if (!m_Operations.Validate)
                validation.Error("pipeline.operation.missing", "The validation operation is missing.", "Validate");
            else
                validation.Append(m_Operations.Validate(snapshot));
            if (!finishStage(BuildPipelineStage::Validate, std::move(validation)))
                return report;

            activeStage = BuildPipelineStage::ResolveContent;
            if (cancelled())
            {
                cancelAt(BuildPipelineStage::ResolveContent);
                return report;
            }

            Path workingRoot;
            String stagingError;
            if (!CreateOwnedSiblingDirectory(report.OutputDirectory, "build", workingRoot, stagingError))
            {
                BuildPipelineStageReport& validate = StageReport(report, BuildPipelineStage::Validate);
                validate.Diagnostics.Error("pipeline.staging.create_failed", "Cannot create the unique staging root: " + stagingError,
                                           workingRoot.string());
                SortIssues(validate.Diagnostics);
                validate.Status = BuildPipelineStageStatus::Failed;
                SkipAfter(report, BuildPipelineStage::Validate);
                return report;
            }
            ScopedDirectoryRemoval workingCleanup;
            workingCleanup.TakeOwnership(workingRoot);

            ContentResolveResult content;
            if (!m_Operations.ResolveContent)
                content.Validation.Error("pipeline.operation.missing", "The content resolver operation is missing.", "Resolve Content");
            else
                content = m_Operations.ResolveContent(snapshot.Content, CreateContentResolveRequest(snapshot.Profile));
            if (!finishStage(BuildPipelineStage::ResolveContent, content.Validation))
                return report;
            std::sort(content.Assets.begin(), content.Assets.end(), [](const ResolvedContentAsset& left, const ResolvedContentAsset& right) {
                const String leftPath = NormalizePathText(left.Asset.LogicalPath);
                const String rightPath = NormalizePathText(right.Asset.LogicalPath);
                return leftPath != rightPath ? leftPath < rightPath : left.Asset.Id.ToString() < right.Asset.Id.ToString();
            });

            Vector<BuildPipelineArtifact> artifacts;
            const bool hasManagedAssembly = !snapshot.Managed.Sources.empty();
            ScopedDirectoryRemoval managedCleanup;
            activeStage = BuildPipelineStage::CompileManaged;
            if (cancelled())
            {
                cancelAt(BuildPipelineStage::CompileManaged);
                return report;
            }
            if (!hasManagedAssembly)
                StageReport(report, BuildPipelineStage::CompileManaged).Status = BuildPipelineStageStatus::Skipped;
            else
            {
                ManagedBuildRequest managed = snapshot.Managed;
                if (managed.ProjectRoot.empty())
                    managed.ProjectRoot = snapshot.ProjectRoot;
                Path managedScratch;
                String scratchError;
                if (!CreateOwnedSiblingDirectory(workingRoot / "ManagedBuild", "pipeline", managedScratch, scratchError))
                {
                    BuildValidation compileValidation;
                    compileValidation.Error("pipeline.managed.staging_failed", "Cannot create managed staging directory: " + scratchError,
                                            managedScratch.string());
                    finishStage(BuildPipelineStage::CompileManaged, std::move(compileValidation));
                    return report;
                }
                managedCleanup.TakeOwnership(managedScratch);
                managed.OutputAssembly = managedScratch / "Game.dll";
                managed.Configuration = snapshot.Target.Configuration;
                managed.Cancellation = cancelled;
                managed.Symbols.insert(managed.Symbols.end(), snapshot.Profile.Symbols.begin(), snapshot.Profile.Symbols.end());
                managed.Symbols.insert(managed.Symbols.end(), snapshot.Target.Symbols.begin(), snapshot.Target.Symbols.end());
                std::sort(managed.Symbols.begin(), managed.Symbols.end());
                managed.Symbols.erase(std::unique(managed.Symbols.begin(), managed.Symbols.end()), managed.Symbols.end());
                ManagedCompileResult compiled;
                if (!m_Operations.CompileManaged)
                    compiled.Diagnostics.push_back({ "pipeline.operation.missing", "The managed compiler operation is missing.", {} });
                else
                    compiled = m_Operations.CompileManaged(managed, snapshot.Toolchain);
                if (compiled.Cancelled || cancelled())
                {
                    cancelAt(BuildPipelineStage::CompileManaged);
                    return report;
                }
                BuildValidation compileValidation;
                for (const ManagedBuildDiagnostic& diagnostic : compiled.Diagnostics)
                    compileValidation.Error(diagnostic.Code, diagnostic.Message, diagnostic.Subject.string());
                if (!compiled.Succeeded() && compileValidation.IsValid())
                    compileValidation.Error("pipeline.managed.failed", "Managed compilation failed without a diagnostic.",
                                            managed.OutputAssembly.string());
                if (compiled.Succeeded() && !fs::is_regular_file(managed.OutputAssembly))
                    compileValidation.Error("pipeline.managed.output_missing", "Managed compilation did not produce its declared output.",
                                            managed.OutputAssembly.string());
                if (compiled.Succeeded() && compileValidation.IsValid() && m_Operations.ValidateProducedArtifacts)
                {
                    const ManagedAssemblyInspection inspection = InspectManagedAssembly(managed.OutputAssembly);
                    for (const ManagedBuildDiagnostic& diagnostic : inspection.Diagnostics)
                        compileValidation.Error(diagnostic.Code, diagnostic.Message, diagnostic.Subject.string());
                    if (inspection.Diagnostics.empty() && !inspection.IsILOnly)
                        compileValidation.Error("pipeline.managed.output_invalid", "The compiler output is not a valid IL-only managed assembly.",
                                                managed.OutputAssembly.string());
                }
                if (!finishStage(BuildPipelineStage::CompileManaged, std::move(compileValidation)))
                    return report;
                artifacts.push_back({ managed.OutputAssembly, "Managed/Game.dll" });
                Path pdb = managed.OutputAssembly;
                pdb.replace_extension(".pdb");
                if (snapshot.Target.IncludeSymbols && fs::is_regular_file(pdb))
                    artifacts.push_back({ pdb, "Managed/Game.pdb" });
                const Path mdb = managed.OutputAssembly.string() + ".mdb";
                if (snapshot.Target.IncludeSymbols && fs::is_regular_file(mdb))
                    artifacts.push_back({ mdb, "Managed/Game.dll.mdb" });
            }

            activeStage = BuildPipelineStage::PackContent;
            if (cancelled())
            {
                cancelAt(BuildPipelineStage::PackContent);
                return report;
            }
            const Path contentPack = workingRoot / "Generated/Content/main.cwpack";
            ContentPackDescriptor descriptor;
            descriptor.EngineVersion = snapshot.EngineVersion;
            descriptor.PlayerAbi = PLAYER_ABI_VERSION;
            descriptor.ContentSchema = CONTENT_SCHEMA_VERSION;
            Vector<ContentPackInput> packInputs;
            BuildValidation packValidation;
            for (const ResolvedContentAsset& asset : content.Assets)
            {
                const Path cooked = ResolveProjectPath(snapshot.ProjectRoot, asset.Asset.CookedPath);
                if (!IsWithin(snapshot.ProjectRoot, cooked) || TraversesLinkOrReparsePoint(snapshot.ProjectRoot, cooked) ||
                    !fs::is_regular_file(cooked))
                    packValidation.Error("pipeline.pack.input_unsafe",
                                         "A resolved cooked input is missing, escapes the project, or traverses a link/reparse point.",
                                         cooked.string());
                packInputs.push_back({ asset.Asset.Id, asset.Asset.LogicalPath, cooked });
            }
            if (packValidation.IsValid())
            {
                if (!m_Operations.PackContent)
                    packValidation.Error("pipeline.operation.missing", "The content pack operation is missing.", "Pack");
                else if (const String error = m_Operations.PackContent(contentPack, descriptor, packInputs); !error.empty())
                    packValidation.Error("pipeline.pack.failed", error, contentPack.string());
                else if (!fs::is_regular_file(contentPack))
                    packValidation.Error("pipeline.pack.output_missing", "Content packing did not produce its declared output.",
                                         contentPack.string());
                else if (m_Operations.ValidateProducedArtifacts)
                    packValidation.Append(ValidateContentPackOutput(contentPack, descriptor, packInputs));
            }
            if (!finishStage(BuildPipelineStage::PackContent, std::move(packValidation)))
                return report;
            artifacts.push_back({ contentPack, "Content/main.cwpack" });

            activeStage = BuildPipelineStage::StageTemplate;
            if (cancelled())
            {
                cancelAt(BuildPipelineStage::StageTemplate);
                return report;
            }
            const Path playerCandidate = workingRoot / "Player";
            PlayerTemplateRequest templateValidation;
            templateValidation.EngineVersion = snapshot.EngineVersion;
            templateValidation.PlayerAbi = PLAYER_ABI_VERSION;
            templateValidation.ContentSchema = CONTENT_SCHEMA_VERSION;
            templateValidation.Platform = snapshot.Target.Platform;
            templateValidation.Configuration = snapshot.Target.Configuration;
            templateValidation.Compatibility = snapshot.Target.Compatibility;
            templateValidation.RequiredRenderers = RequiredRenderers(snapshot.Target.Renderers);
            BuildValidation stageValidation;
            if (!m_Operations.StageTemplate)
                stageValidation.Error("pipeline.operation.missing", "The player template staging operation is missing.", "Stage Template");
            else
                stageValidation =
                  m_Operations.StageTemplate({ snapshot.TemplateRoot, snapshot.Template, templateValidation, playerCandidate, artifacts });
            if (stageValidation.IsValid())
            {
                if (!fs::is_directory(playerCandidate))
                    stageValidation.Error("pipeline.template.output_missing", "Template staging did not produce the player directory.",
                                          playerCandidate.string());
                for (const BuildPipelineArtifact& artifact : artifacts)
                {
                    const Path staged = playerCandidate / artifact.RelativeDestination;
                    if (!fs::is_regular_file(staged))
                        stageValidation.Error("pipeline.artifact.output_missing", "Template staging omitted a generated artifact.",
                                              artifact.RelativeDestination.generic_string());
                    else
                    {
                        const String stagedHash = ComputeFileSha256(staged);
                        const String sourceHash = ComputeFileSha256(artifact.Source);
                        if (stagedHash.empty() || sourceHash.empty() || stagedHash != sourceHash)
                            stageValidation.Error("pipeline.artifact.hash_mismatch", "A staged artifact does not match its source.",
                                                  artifact.RelativeDestination.generic_string());
                    }
                }
                if (TraversesLinkOrReparsePoint(workingRoot, playerCandidate))
                    stageValidation.Error("pipeline.template.output_unsafe", "The staged player traverses a symbolic link or reparse point.",
                                          playerCandidate.string());
                stageValidation.Append(ValidateDirectoryTree(playerCandidate, "pipeline.template.output_unsafe"));
                if (m_Operations.ValidateProducedArtifacts)
                    stageValidation.Append(ValidateTemplateFilesAt(playerCandidate, snapshot.Template));
            }
            if (!finishStage(BuildPipelineStage::StageTemplate, std::move(stageValidation)))
                return report;

            activeStage = BuildPipelineStage::WriteManifest;
            if (cancelled())
            {
                cancelAt(BuildPipelineStage::WriteManifest);
                return report;
            }
            const BuildManifest manifest = CreateManifest(snapshot, content, hasManagedAssembly);
            BuildValidation manifestValidation = ValidateBuildManifest(manifest);
            if (manifestValidation.IsValid())
            {
                if (!m_Operations.WriteManifest)
                    manifestValidation.Error("pipeline.operation.missing", "The manifest writer operation is missing.", "Write Manifest");
                else if (const String error = m_Operations.WriteManifest(playerCandidate / "BuildManifest.yaml", manifest); !error.empty())
                    manifestValidation.Error("pipeline.manifest.write_failed", error, (playerCandidate / "BuildManifest.yaml").string());
                else if (!fs::is_regular_file(playerCandidate / "BuildManifest.yaml"))
                    manifestValidation.Error("pipeline.manifest.output_missing", "Manifest writing did not produce its declared output.",
                                             (playerCandidate / "BuildManifest.yaml").string());
                else if (m_Operations.ValidateProducedArtifacts)
                    manifestValidation.Append(ValidateManifestOutput(playerCandidate / "BuildManifest.yaml", manifest));
            }
            if (!finishStage(BuildPipelineStage::WriteManifest, std::move(manifestValidation)))
                return report;

            activeStage = BuildPipelineStage::Publish;
            if (cancelled())
            {
                cancelAt(BuildPipelineStage::Publish);
                return report;
            }
            BuildValidation publishValidation = PublishDirectory(playerCandidate, report.OutputDirectory, m_Operations, [&](const Path& output) {
                BuildValidation result = ValidateDirectoryTree(output, "pipeline.publish.output_unsafe");
                result.Append(ValidateGeneratedArtifactsAt(output, artifacts));
                if (m_Operations.ValidateProducedArtifacts)
                {
                    result.Append(ValidateTemplateFilesAt(output, snapshot.Template));
                    result.Append(ValidateContentPackOutput(output / "Content/main.cwpack", descriptor, packInputs));
                    result.Append(ValidateManifestOutput(output / "BuildManifest.yaml", manifest));
                }
                return result;
            });
            finishStage(BuildPipelineStage::Publish, std::move(publishValidation));
            return report;
        }
        catch (const CancellationCheckException& exception)
        {
            BuildPipelineStageReport& stage = StageReport(report, activeStage);
            stage.Status = BuildPipelineStageStatus::Failed;
            stage.Diagnostics.Error("pipeline.cancellation.exception", "The cancellation predicate threw an exception: " + String(exception.what()),
                                    ToString(activeStage));
            SortIssues(stage.Diagnostics);
            SkipAfter(report, activeStage);
        }
        catch (const std::exception& exception)
        {
            BuildPipelineStageReport& stage = StageReport(report, activeStage);
            stage.Status = BuildPipelineStageStatus::Failed;
            stage.Diagnostics.Error("pipeline.stage.exception",
                                    "The " + String(ToString(activeStage)) + " stage threw an exception: " + exception.what(), ToString(activeStage));
            SortIssues(stage.Diagnostics);
            SkipAfter(report, activeStage);
        }
        catch (...)
        {
            BuildPipelineStageReport& stage = StageReport(report, activeStage);
            stage.Status = BuildPipelineStageStatus::Failed;
            stage.Diagnostics.Error("pipeline.stage.exception", "The " + String(ToString(activeStage)) + " stage threw an unknown exception.",
                                    ToString(activeStage));
            SortIssues(stage.Diagnostics);
            SkipAfter(report, activeStage);
        }
        return report;
    }
} // namespace Crowny
