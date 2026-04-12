#include "cwpch.h"

#include "Crowny/Utils/BuiltInShaderCompiler.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Timer.h"
#include "Crowny/RenderAPI/Shader.h"

namespace Crowny
{

    // Get file modification time as epoch seconds
    static int64_t GetSourceTimestamp(const Path& path)
    {
        if (!fs::exists(path))
            return 0;
        auto ftime = fs::last_write_time(path);
        auto duration = ftime.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    }

    static uint64_t HashFileContent(const String& str) { return std::hash<String>{}(str); }

    bool BuiltInShaderCompiler::NeedsRecompile(const Path& glslPath, const Path& assetPath)
    {
        if (!fs::exists(assetPath))
            return true;

        // The .asset header is embedded inside cereal's polymorphic serialization framing,
        // so it cannot be read by peeking at byte 0 of the file. Use modification time instead.
        std::error_code ec;
        auto glslTime = fs::last_write_time(glslPath, ec);
        if (ec)
            return true;
        auto assetTime = fs::last_write_time(assetPath, ec);
        if (ec)
            return true;
        if (glslTime > assetTime)
            return true;

        // Guard against broken .asset files written by a previous failed compilation.
        // A valid compiled shader (vertex+fragment SPIRV) is always well over 1 KB.
        // An asset written with empty SPIRV data (Shader::Create never returns null) is ~200–400 bytes.
        auto fileSize = fs::file_size(assetPath, ec);
        if (ec || fileSize < 1024)
            return true;

        return false;
    }

    bool BuiltInShaderCompiler::CompileAndSave(const Path& glslPath, const Path& assetPath)
    {
        Ref<DataStream> stream = FileSystem::OpenFile(glslPath);
        if (!stream)
        {
            CW_ENGINE_ERROR("BuiltInShaderCompiler: Failed to open {}", glslPath.string());
            return false;
        }

        const String source = stream->GetAsString();
        stream->Close();

        ShaderDesc desc = ShaderCompiler::Compile(glslPath, source);

        // Shader::Create never returns null — validate SPIRV before saving
        bool hasValidPass = false;
        for (const auto& technique : desc.Techniques)
        {
            for (const auto& pass : technique->GetRenderPasses())
            {
                const ShaderRenderPassDesc& pd = pass->GetPassDesc();
                bool graphicsOk = pd.VertexShader && !pd.VertexShader->Data.empty() && pd.FragmentShader && !pd.FragmentShader->Data.empty();
                bool computeOk = pd.ComputeShader && !pd.ComputeShader->Data.empty();
                if (graphicsOk || computeOk)
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
            CW_ENGINE_ERROR("BuiltInShaderCompiler: {} produced no valid SPIRV — not saved", glslPath.filename().string());
            return false;
        }

        Ref<Shader> shader = Shader::Create(desc);

        // Store source tracking on the asset so the header includes it when saved
        shader->SetSourceTimestamp(GetSourceTimestamp(glslPath));
        shader->SetSourceContentHash(HashFileContent(source));

        gAssetManager->Save(shader, assetPath);
        return true;
    }

    void BuiltInShaderCompiler::CompileAll()
    {
        Timer timer;
        const Path sourceDir(SHADER_SOURCE_DIR);

        if (!fs::exists(sourceDir) || !fs::is_directory(sourceDir))
        {
            CW_ENGINE_WARN("BuiltInShaderCompiler: Shader source directory '{}' not found.", sourceDir.string());
            return;
        }

        uint32_t compiled = 0;
        uint32_t skipped = 0;
        uint32_t failed = 0;

        for (const auto& entry : fs::directory_iterator(sourceDir))
        {
            if (!entry.is_regular_file())
                continue;

            const Path& glslPath = entry.path();
            if (glslPath.extension() != ".glsl")
                continue;

            Path assetPath = glslPath;
            assetPath.replace_extension(".asset");

            // Skip legacy OpenGL shaders — they lack the #lang directive and
            // cannot be compiled to SPIR-V without manual porting.
            {
                Ref<DataStream> peek = FileSystem::OpenFile(glslPath);
                if (!peek)
                    continue;
                const String src = peek->GetAsString();
                peek->Close();
                if (src.find("#lang") == String::npos)
                {
                    skipped++;
                    continue;
                }
            }

            if (!NeedsRecompile(glslPath, assetPath))
            {
                skipped++;
                continue;
            }

            CW_ENGINE_INFO("Compiling built-in shader: {}", glslPath.filename().string());
            if (CompileAndSave(glslPath, assetPath))
                compiled++;
            else
                failed++;
        }

        if (compiled > 0 || failed > 0)
            CW_ENGINE_INFO("Built-in shaders: {} compiled, {} up-to-date, {} failed.", compiled, skipped, failed);
    }

} // namespace Crowny
