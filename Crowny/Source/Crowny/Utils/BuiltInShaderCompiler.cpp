#include "cwpch.h"

#include "Crowny/Utils/BuiltInShaderCompiler.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetCodecs.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/RenderAPI/Shader.h"

namespace Crowny
{
    namespace
    {
        int64_t GetSourceTimestamp(const Path& path)
        {
            std::error_code error;
            const auto writeTime = fs::last_write_time(path, error);
            if (error)
                return 0;
            return std::chrono::duration_cast<std::chrono::seconds>(writeTime.time_since_epoch()).count();
        }

        int64_t GetLatestSourceTimestamp(const Path& root, const Vector<Path>& dependencies)
        {
            int64_t latest = GetSourceTimestamp(root);
            for (const Path& dependency : dependencies)
                latest = std::max(latest, GetSourceTimestamp(dependency));
            return latest;
        }

        bool SynchronizeAssetWriteTime(const Path& assetPath, const Path& root, const Vector<Path>& dependencies)
        {
            std::error_code error;
            fs::file_time_type latestSourceTime = fs::last_write_time(root, error);
            if (error)
                return false;

            for (const Path& dependency : dependencies)
            {
                const fs::file_time_type dependencyTime = fs::last_write_time(dependency, error);
                if (error)
                {
                    error.clear();
                    continue;
                }
                latestSourceTime = std::max(latestSourceTime, dependencyTime);
            }

            const fs::file_time_type assetTime = fs::last_write_time(assetPath, error);
            if (error || assetTime >= latestSourceTime)
                return !error;

            fs::last_write_time(assetPath, latestSourceTime, error);
            if (error)
                CW_ENGINE_WARN("BuiltInShaderCompiler: Failed to synchronize asset timestamp for '{}'", assetPath.string());
            return !error;
        }

        void LogDiagnostics(const Path& root, const Vector<ShaderDiagnostic>& diagnostics)
        {
            for (const ShaderDiagnostic& diagnostic : diagnostics)
            {
                const Path& file = diagnostic.File.empty() ? root : diagnostic.File;
                const String location = diagnostic.Line == 0 ? file.string() : file.string() + ":" + std::to_string(diagnostic.Line);
                if (diagnostic.Severity == ShaderDiagnosticSeverity::Error)
                    CW_ENGINE_ERROR("{}: {}", location, diagnostic.Message);
                else
                    CW_ENGINE_WARN("{}: {}", location, diagnostic.Message);
            }
        }
    } // namespace

    bool BuiltInShaderCompiler::NeedsRecompile(const Path& assetPath, uint64_t sourceContentHash)
    {
        AssetFileHeader header;
        if (!PeekAssetHeader(assetPath, header) || header.Type != AssetType::Shader || header.Version != SHADER_FORMAT_VERSION ||
            header.SourceContentHash != sourceContentHash)
            return true;

        // An asset containing valid vertex and fragment SPIR-V is well over 1 KB.
        // Keep the size guard for files left behind by an interrupted save.
        std::error_code error;
        const uintmax_t fileSize = fs::file_size(assetPath, error);
        return error || fileSize < 1024;
    }

    bool BuiltInShaderCompiler::CompileAndSave(const Path& glslPath, const Path& assetPath, const String& source)
    {
        ShaderCompileResult compileResult = ShaderCompiler::CompileWithDiagnostics(glslPath, source);
        LogDiagnostics(glslPath, compileResult.Diagnostics);
        if (!compileResult.Succeeded())
            return false;
        ShaderDesc& desc = compileResult.Description;

        // Shader::Create does not reject empty binary data, so validate before saving.
        bool hasValidPass = false;
        for (const auto& technique : desc.Techniques)
        {
            for (const auto& pass : technique->GetRenderPasses())
            {
                const ShaderRenderPassDesc& passDesc = pass->GetPassDesc();
                const bool graphicsValid = passDesc.VertexShader && !passDesc.VertexShader->Data.empty() && passDesc.FragmentShader &&
                                           !passDesc.FragmentShader->Data.empty();
                const bool computeValid = passDesc.ComputeShader && !passDesc.ComputeShader->Data.empty();
                if (graphicsValid || computeValid)
                {
                    hasValidPass = true;
                    break;
                }
            }
            if (hasValidPass)
                break;
        }
        if (!hasValidPass)
        {
            CW_ENGINE_ERROR("BuiltInShaderCompiler: {} produced no valid SPIR-V and was not saved", glslPath.filename().string());
            return false;
        }

        AssetManager* assetManager = AssetManager::TryGet();
        if (assetManager == nullptr)
        {
            CW_ENGINE_ERROR("BuiltInShaderCompiler: AssetManager is not available");
            return false;
        }

        Ref<Shader> shader = Shader::Create(desc);
        shader->SetSourceTimestamp(GetLatestSourceTimestamp(glslPath, compileResult.Dependencies));
        shader->SetSourceContentHash(compileResult.SourceContentHash);
        assetManager->Save(shader, assetPath);

        AssetFileHeader savedHeader;
        const bool saved = PeekAssetHeader(assetPath, savedHeader) && savedHeader.Type == AssetType::Shader &&
                           savedHeader.Version == SHADER_FORMAT_VERSION &&
                           savedHeader.SourceContentHash == compileResult.SourceContentHash;
        if (!saved)
            CW_ENGINE_ERROR("BuiltInShaderCompiler: Failed to verify saved shader asset '{}'", assetPath.string());
        return saved;
    }

    void BuiltInShaderCompiler::CompileAll()
    {
        Path sourceDirectory(SHADER_SOURCE_DIR);
        if (!fs::is_directory(sourceDirectory) && Application::TryGet() != nullptr)
        {
            const Path editorSourceDirectory = Application::TryGet()->GetWorkingDirectory() / "Crowny-Editor" / SHADER_SOURCE_DIR;
            if (fs::is_directory(editorSourceDirectory))
                sourceDirectory = editorSourceDirectory;
        }

        const BuiltInShaderCompileStats stats = CompileAll(sourceDirectory);
        if (stats.Compiled > 0 || stats.Failed > 0)
            CW_ENGINE_INFO("Built-in shaders: {} compiled, {} up-to-date, {} failed.", stats.Compiled, stats.Skipped, stats.Failed);
    }

    BuiltInShaderCompileStats BuiltInShaderCompiler::CompileAll(const Path& sourceDirectory)
    {
        BuiltInShaderCompileStats stats;
        if (!fs::exists(sourceDirectory) || !fs::is_directory(sourceDirectory))
        {
            CW_ENGINE_WARN("BuiltInShaderCompiler: Shader source directory '{}' not found.", sourceDirectory.string());
            return stats;
        }

        Vector<Path> shaderPaths;
        for (const auto& entry : fs::directory_iterator(sourceDirectory))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".glsl")
                shaderPaths.push_back(entry.path());
        }
        std::sort(shaderPaths.begin(), shaderPaths.end());

        for (const Path& glslPath : shaderPaths)
        {
            Path assetPath = glslPath;
            assetPath.replace_extension(".asset");

            const Ref<DataStream> stream = FileSystem::OpenFile(glslPath);
            if (!stream)
            {
                CW_ENGINE_ERROR("BuiltInShaderCompiler: Failed to open {}", glslPath.string());
                stats.Failed++;
                continue;
            }
            const String source = stream->GetAsString();
            stream->Close();

            // Legacy OpenGL shaders do not declare an input language and cannot
            // be compiled to SPIR-V without manual porting.
            if (source.find("#lang") == String::npos)
            {
                stats.Skipped++;
                continue;
            }

            const ShaderPreprocessResult preprocessed = ShaderCompiler::PreprocessIncludes(glslPath, source);
            if (!preprocessed.Succeeded())
            {
                LogDiagnostics(glslPath, preprocessed.Diagnostics);
                stats.Failed++;
                continue;
            }
            if (!NeedsRecompile(assetPath, preprocessed.ContentHash))
            {
                // Packaging retains a cheap timestamp check. Keep it in sync with
                // the content-hash result without recompiling unchanged shaders.
                if (SynchronizeAssetWriteTime(assetPath, glslPath, preprocessed.Dependencies))
                    stats.Skipped++;
                else
                    stats.Failed++;
                continue;
            }

            CW_ENGINE_INFO("Compiling built-in shader: {}", glslPath.filename().string());
            if (CompileAndSave(glslPath, assetPath, source))
            {
                if (SynchronizeAssetWriteTime(assetPath, glslPath, preprocessed.Dependencies))
                    stats.Compiled++;
                else
                    stats.Failed++;
            }
            else
                stats.Failed++;
        }
        return stats;
    }
} // namespace Crowny
