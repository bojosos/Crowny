#include "cwpch.h"

#include "Crowny/Build/BuilderCli.h"
#include "Crowny/Common/Version.h"
#include "Crowny/Common/Yaml.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <iostream>
#include <limits>

#ifdef CW_PLATFORM_WIN32
#include <Windows.h>
#endif

namespace Crowny
{
    namespace
    {
        constexpr uint32_t BUILDER_REQUEST_SCHEMA = 1;

        enum class BuilderOutputFormat
        {
            Text,
            Json
        };

        struct BuilderCommandLine
        {
            Path RequestFile;
            Path ReportFile;
            BuilderOutputFormat Format = BuilderOutputFormat::Text;
            bool ShowHelp = false;
            bool ShowVersion = false;
        };

        struct BuilderError
        {
            String Code;
            String Message;
            String Subject;
        };

        struct BuilderLoadResult
        {
            BuildPipelineRequest Request;
            BuilderError Error;

            bool Succeeded() const { return Error.Code.empty(); }
        };

        String Usage()
        {
            return "Crowny-Builder build --request <file> [--report <file>] [--format text|json]\n"
                   "\n"
                   "Commands:\n"
                   "  build                 Build one target from an editor-generated request file.\n"
                   "\n"
                   "Options:\n"
                   "  --request <file>      YAML request snapshot for one build target.\n"
                   "  --report <file>       Write the structured JSON result atomically.\n"
                   "  --format text|json    Select human-readable or machine-readable stdout.\n"
                   "  --help, -h            Show this help.\n"
                   "  --version             Print the Crowny engine version.\n";
        }

        bool IsOption(const String& value, StringView name)
        {
            return value == name || (value.size() > name.size() && value.starts_with(name) && value[name.size()] == '=');
        }

        String OptionValue(const Vector<String>& arguments, size_t& index, StringView name, BuilderError& error)
        {
            const String& argument = arguments[index];
            if (argument.size() > name.size())
                return argument.substr(name.size() + 1);
            if (index + 1 >= arguments.size())
            {
                error = { "builder.command.value_missing", "Option '" + String(name) + "' needs a value.", String(name) };
                return {};
            }
            return arguments[++index];
        }

        bool WantsJson(const Vector<String>& arguments)
        {
            for (size_t index = 0; index < arguments.size(); index++)
            {
                if (!IsOption(arguments[index], "--format"))
                    continue;
                if (arguments[index] == "--format")
                    return index + 1 < arguments.size() && arguments[index + 1] == "json";
                return arguments[index].substr(String("--format=").size()) == "json";
            }
            return false;
        }

        bool ParseCommandLine(const Vector<String>& arguments, BuilderCommandLine& command, BuilderError& error)
        {
            command.Format = WantsJson(arguments) ? BuilderOutputFormat::Json : BuilderOutputFormat::Text;
            if (arguments.size() == 1 && (arguments[0] == "--help" || arguments[0] == "-h"))
            {
                command.ShowHelp = true;
                return true;
            }
            if (arguments.size() == 1 && arguments[0] == "--version")
            {
                command.ShowVersion = true;
                return true;
            }
            if (arguments.empty() || arguments[0] != "build")
            {
                error = { "builder.command.invalid", "Expected the 'build' command.", arguments.empty() ? String() : arguments[0] };
                return false;
            }

            bool requestSeen = false;
            bool reportSeen = false;
            bool formatSeen = false;
            for (size_t index = 1; index < arguments.size(); index++)
            {
                const String& argument = arguments[index];
                if (argument == "--help" || argument == "-h")
                {
                    command.ShowHelp = true;
                    continue;
                }
                if (IsOption(argument, "--request"))
                {
                    if (requestSeen)
                    {
                        error = { "builder.command.duplicate", "Option '--request' can only be given once.", "--request" };
                        return false;
                    }
                    requestSeen = true;
                    command.RequestFile = OptionValue(arguments, index, "--request", error);
                }
                else if (IsOption(argument, "--report"))
                {
                    if (reportSeen)
                    {
                        error = { "builder.command.duplicate", "Option '--report' can only be given once.", "--report" };
                        return false;
                    }
                    reportSeen = true;
                    command.ReportFile = OptionValue(arguments, index, "--report", error);
                }
                else if (IsOption(argument, "--format"))
                {
                    if (formatSeen)
                    {
                        error = { "builder.command.duplicate", "Option '--format' can only be given once.", "--format" };
                        return false;
                    }
                    formatSeen = true;
                    const String value = OptionValue(arguments, index, "--format", error);
                    if (!error.Code.empty())
                        return false;
                    if (value == "text")
                        command.Format = BuilderOutputFormat::Text;
                    else if (value == "json")
                        command.Format = BuilderOutputFormat::Json;
                    else
                    {
                        error = { "builder.command.format_invalid", "Output format must be 'text' or 'json'.", value };
                        return false;
                    }
                }
                else
                {
                    error = { "builder.command.option_unknown", "Unknown option '" + argument + "'.", argument };
                    return false;
                }
                if (!error.Code.empty())
                    return false;
            }

            if (command.ShowHelp)
                return true;
            if (command.RequestFile.empty())
            {
                error = { "builder.command.request_missing", "Option '--request' is required.", "--request" };
                return false;
            }
            if (command.RequestFile.filename().empty())
            {
                error = { "builder.command.request_invalid", "The request path must name a file.", command.RequestFile.string() };
                return false;
            }
            if (!command.ReportFile.empty() && command.ReportFile.filename().empty())
            {
                error = { "builder.command.report_invalid", "The report path must name a file.", command.ReportFile.string() };
                return false;
            }
            return true;
        }

        Path AbsolutePath(const Path& base, const Path& value)
        {
            const Path combined = value.is_absolute() ? value : base / value;
            std::error_code error;
            const Path absolute = fs::absolute(combined, error);
            return (error ? combined : absolute).lexically_normal();
        }

        bool CheckKeys(const YAML::Node& node, std::initializer_list<StringView> allowed, StringView area, BuilderError& error)
        {
            if (!node.IsMap())
            {
                error = { "builder.request.map_expected", String(area) + " must be a map.", String(area) };
                return false;
            }
            for (const auto& entry : node)
            {
                const String key = entry.first.as<String>();
                if (std::find(allowed.begin(), allowed.end(), key) == allowed.end())
                {
                    error = { "builder.request.key_unknown", "Unknown " + String(area) + " key '" + key + "'.", key };
                    return false;
                }
            }
            return true;
        }

        bool ReadRequiredString(const YAML::Node& node, const char* key, String& output, BuilderError& error)
        {
            const YAML::Node value = node[key];
            if (!value || !value.IsScalar())
            {
                error = { "builder.request.value_missing", "Request value '" + String(key) + "' is required.", key };
                return false;
            }
            output = value.as<String>();
            if (output.empty())
            {
                error = { "builder.request.value_empty", "Request value '" + String(key) + "' cannot be empty.", key };
                return false;
            }
            return true;
        }

        bool ReadPathSequence(const YAML::Node& node, const char* key, Vector<Path>& output, BuilderError& error)
        {
            const YAML::Node values = node[key];
            if (!values)
                return true;
            if (!values.IsSequence())
            {
                error = { "builder.request.sequence_expected", "Managed value '" + String(key) + "' must be a sequence.", key };
                return false;
            }
            for (const YAML::Node value : values)
            {
                if (!value.IsScalar() || value.as<String>().empty())
                {
                    error = { "builder.request.path_invalid", "Managed path lists cannot contain empty or non-scalar values.", key };
                    return false;
                }
                output.emplace_back(value.as<String>());
            }
            return true;
        }

        bool ReadStringSequence(const YAML::Node& node, const char* key, Vector<String>& output, BuilderError& error)
        {
            const YAML::Node values = node[key];
            if (!values)
                return true;
            if (!values.IsSequence())
            {
                error = { "builder.request.sequence_expected", "Managed value '" + String(key) + "' must be a sequence.", key };
                return false;
            }
            for (const YAML::Node value : values)
            {
                if (!value.IsScalar() || value.as<String>().empty())
                {
                    error = { "builder.request.symbol_invalid", "Managed symbols cannot contain empty or non-scalar values.", key };
                    return false;
                }
                output.push_back(value.as<String>());
            }
            return true;
        }

        BuilderLoadResult LoadRequest(const Path& requestFile)
        {
            BuilderLoadResult result;
            try
            {
                const Path absoluteRequest = AbsolutePath(fs::current_path(), requestFile);
                const YAML::Node root = YAML::LoadFile(absoluteRequest.string());
                if (!CheckKeys(root,
                               { "Schema", "ProjectRoot", "OutputDirectory", "GameSettings", "BuildProfile", "BuildTarget",
                                 "ContentDatabase", "Managed", "TemplateRoot", "TemplateManifest", "EngineVersion", "MonoVersion" },
                               "request", result.Error))
                    return result;

                const uint32_t schema = root["Schema"].as<uint32_t>(0);
                if (schema != BUILDER_REQUEST_SCHEMA)
                {
                    result.Error = { "builder.request.schema_unsupported",
                                     "Builder request schema " + std::to_string(schema) + " is not supported.", std::to_string(schema) };
                    return result;
                }

                String value;
                if (!ReadRequiredString(root, "ProjectRoot", value, result.Error))
                    return result;
                BuildPipelineRequest& request = result.Request;
                request.ProjectRoot = AbsolutePath(absoluteRequest.parent_path(), value);
                if (!fs::is_directory(request.ProjectRoot))
                {
                    result.Error = { "builder.request.project_missing", "ProjectRoot does not name an existing directory.",
                                     request.ProjectRoot.string() };
                    return result;
                }

                if (!ReadRequiredString(root, "OutputDirectory", value, result.Error))
                    return result;
                request.OutputDirectory = AbsolutePath(request.ProjectRoot, value);

                String gameSettingsPath = "ProjectSettings/Game.yaml";
                if (const YAML::Node gameSettings = root["GameSettings"])
                    gameSettingsPath = gameSettings.as<String>();
                if (gameSettingsPath.empty())
                {
                    result.Error = { "builder.request.game_settings_empty", "GameSettings cannot be empty.", "GameSettings" };
                    return result;
                }
                const Path gameSettings = AbsolutePath(request.ProjectRoot, gameSettingsPath);
                if (const String loadError = BuildProfileStore::LoadGameSettings(gameSettings, request.Game); !loadError.empty())
                {
                    result.Error = { "builder.request.game_settings_invalid", loadError, gameSettings.string() };
                    return result;
                }

                if (!ReadRequiredString(root, "BuildProfile", value, result.Error))
                    return result;
                const Path profilePath = AbsolutePath(request.ProjectRoot, value);
                if (const String loadError = BuildProfileStore::LoadProfile(profilePath, request.Profile); !loadError.empty())
                {
                    result.Error = { "builder.request.profile_invalid", loadError, profilePath.string() };
                    return result;
                }

                String targetText;
                if (!ReadRequiredString(root, "BuildTarget", targetText, result.Error))
                    return result;
                const UUID targetId(targetText);
                if (targetId.Empty())
                {
                    result.Error = { "builder.request.target_invalid", "BuildTarget must be a non-empty UUID.", targetText };
                    return result;
                }
                const auto target = std::find_if(request.Profile.Targets.begin(), request.Profile.Targets.end(),
                                                 [&](const BuildTarget& candidate) { return candidate.Id == targetId; });
                if (target == request.Profile.Targets.end())
                {
                    result.Error = { "builder.request.target_missing", "BuildTarget is not present in the selected build profile.", targetText };
                    return result;
                }
                request.Target = *target;

                String contentDatabasePath = "Internal/Build/ContentDatabase.yaml";
                if (const YAML::Node contentDatabase = root["ContentDatabase"])
                    contentDatabasePath = contentDatabase.as<String>();
                if (contentDatabasePath.empty())
                {
                    result.Error = { "builder.request.content_database_empty", "ContentDatabase cannot be empty.", "ContentDatabase" };
                    return result;
                }
                const Path contentDatabase = AbsolutePath(request.ProjectRoot, contentDatabasePath);
                if (const String loadError = ContentDatabaseStore::Load(contentDatabase, request.Content); !loadError.empty())
                {
                    result.Error = { "builder.request.content_database_invalid", loadError, contentDatabase.string() };
                    return result;
                }

                request.Managed.ProjectRoot = request.ProjectRoot;
                if (const YAML::Node managed = root["Managed"])
                {
                    if (!CheckKeys(managed,
                                   { "ToolchainRoot", "Sources", "References", "Symbols", "LanguageVersion", "TimeoutMilliseconds",
                                     "MaxCapturedOutputBytes" },
                                   "Managed", result.Error))
                        return result;
                    if (!ReadPathSequence(managed, "Sources", request.Managed.Sources, result.Error) ||
                        !ReadPathSequence(managed, "References", request.Managed.References, result.Error) ||
                        !ReadStringSequence(managed, "Symbols", request.Managed.Symbols, result.Error))
                        return result;
                    if (const YAML::Node languageVersion = managed["LanguageVersion"])
                        request.Managed.LanguageVersion = languageVersion.as<String>();
                    if (request.Managed.LanguageVersion.empty())
                    {
                        result.Error = { "builder.request.language_version_empty", "Managed LanguageVersion cannot be empty.", "LanguageVersion" };
                        return result;
                    }
                    if (const YAML::Node timeout = managed["TimeoutMilliseconds"])
                    {
                        const uint64_t milliseconds = timeout.as<uint64_t>();
                        if (milliseconds == 0 || milliseconds > 30ULL * 60ULL * 1000ULL)
                        {
                            result.Error = { "builder.request.timeout_invalid",
                                             "Managed TimeoutMilliseconds must be between 1 and 1800000.", std::to_string(milliseconds) };
                            return result;
                        }
                        request.Managed.Timeout = std::chrono::milliseconds(milliseconds);
                    }
                    if (const YAML::Node captured = managed["MaxCapturedOutputBytes"])
                    {
                        const uint64_t bytes = captured.as<uint64_t>();
                        if (bytes < 4096 || bytes > 64ULL * 1024ULL * 1024ULL || bytes > std::numeric_limits<size_t>::max())
                        {
                            result.Error = { "builder.request.capture_limit_invalid",
                                             "Managed MaxCapturedOutputBytes must be between 4096 and 67108864.", std::to_string(bytes) };
                            return result;
                        }
                        request.Managed.MaxCapturedOutputBytes = static_cast<size_t>(bytes);
                    }

                    String toolchainRoot;
                    if (const YAML::Node toolchain = managed["ToolchainRoot"])
                        toolchainRoot = toolchain.as<String>();
                    if (!request.Managed.Sources.empty() && toolchainRoot.empty())
                    {
                        result.Error = { "builder.request.toolchain_missing",
                                         "Managed ToolchainRoot is required when the request contains C# sources.", "ToolchainRoot" };
                        return result;
                    }
                    if (!toolchainRoot.empty())
                        request.Toolchain = LocateManagedToolchain(AbsolutePath(request.ProjectRoot, toolchainRoot));
                }

                if (!ReadRequiredString(root, "TemplateRoot", value, result.Error))
                    return result;
                request.TemplateRoot = AbsolutePath(request.ProjectRoot, value);
                String templateManifestPath = "template.yaml";
                if (const YAML::Node manifest = root["TemplateManifest"])
                    templateManifestPath = manifest.as<String>();
                if (templateManifestPath.empty())
                {
                    result.Error = { "builder.request.template_manifest_empty", "TemplateManifest cannot be empty.", "TemplateManifest" };
                    return result;
                }
                const Path templateManifest = AbsolutePath(request.TemplateRoot, templateManifestPath);
                if (const String loadError = PlayerTemplateStore::Load(templateManifest, request.Template); !loadError.empty())
                {
                    result.Error = { "builder.request.template_invalid", loadError, templateManifest.string() };
                    return result;
                }

                request.EngineVersion = CROWNY_VERSION_STRING;
                if (const YAML::Node engineVersion = root["EngineVersion"])
                {
                    const String requestedVersion = engineVersion.as<String>();
                    if (requestedVersion != request.EngineVersion)
                    {
                        result.Error = { "builder.request.engine_version_mismatch",
                                         "EngineVersion must match this Crowny Builder binary (" + request.EngineVersion + ").",
                                         requestedVersion };
                        return result;
                    }
                }
                if (const YAML::Node monoVersion = root["MonoVersion"])
                    request.MonoVersion = monoVersion.as<String>();
                if (request.MonoVersion.empty() && !request.Toolchain.Version.empty())
                    request.MonoVersion = request.Toolchain.Version;
                if (request.MonoVersion.empty())
                {
                    result.Error = { "builder.request.mono_version_missing",
                                     "MonoVersion is required when the managed toolchain cannot provide it.", "MonoVersion" };
                    return result;
                }
                return result;
            }
            catch (const std::exception& exception)
            {
                result.Error = { "builder.request.read_failed", "Cannot load builder request: " + String(exception.what()), requestFile.string() };
                return result;
            }
        }

        template <class Writer> void WriteErrorObject(Writer& writer, const BuilderError& error)
        {
            writer.StartObject();
            writer.Key("code");
            writer.String(error.Code.c_str());
            writer.Key("message");
            writer.String(error.Message.c_str());
            writer.Key("subject");
            writer.String(error.Subject.c_str());
            writer.EndObject();
        }

        String SerializeError(BuilderExitCode exitCode, const BuilderError& error)
        {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            writer.StartObject();
            writer.Key("schema");
            writer.Uint(1);
            writer.Key("kind");
            writer.String("error");
            writer.Key("succeeded");
            writer.Bool(false);
            writer.Key("exitCode");
            writer.Int(static_cast<int>(exitCode));
            writer.Key("error");
            WriteErrorObject(writer, error);
            writer.EndObject();
            return buffer.GetString();
        }

        String SerializeReport(const BuildPipelineReport& report, BuilderExitCode exitCode)
        {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            writer.StartObject();
            writer.Key("schema");
            writer.Uint(1);
            writer.Key("kind");
            writer.String("build");
            writer.Key("succeeded");
            writer.Bool(report.Succeeded());
            writer.Key("cancelled");
            writer.Bool(report.Cancelled);
            writer.Key("exitCode");
            writer.Int(static_cast<int>(exitCode));
            writer.Key("fingerprint");
            writer.String(report.Fingerprint.c_str());
            writer.Key("outputDirectory");
            const String outputDirectory = report.OutputDirectory.generic_string();
            writer.String(outputDirectory.c_str());
            writer.Key("stages");
            writer.StartArray();
            for (const BuildPipelineStageReport& stage : report.Stages)
            {
                writer.StartObject();
                writer.Key("name");
                writer.String(ToString(stage.Stage));
                writer.Key("status");
                writer.String(ToString(stage.Status));
                writer.Key("issues");
                writer.StartArray();
                for (const BuildIssue& issue : stage.Diagnostics.Issues)
                {
                    writer.StartObject();
                    writer.Key("severity");
                    writer.String(issue.Severity == BuildIssueSeverity::Warning ? "warning" : "error");
                    writer.Key("code");
                    writer.String(issue.Code.c_str());
                    writer.Key("message");
                    writer.String(issue.Message.c_str());
                    writer.Key("subject");
                    writer.String(issue.Subject.c_str());
                    writer.EndObject();
                }
                writer.EndArray();
                writer.EndObject();
            }
            writer.EndArray();
            writer.EndObject();
            return buffer.GetString();
        }

        String WriteReportAtomically(const Path& path, StringView contents)
        {
            if (path.empty() || path.filename().empty())
                return "The report path must name a file.";
            const Path absolute = AbsolutePath(fs::current_path(), path);
            std::error_code error;
            if (!absolute.parent_path().empty())
                fs::create_directories(absolute.parent_path(), error);
            if (error)
                return "Cannot create report directory '" + absolute.parent_path().string() + "': " + error.message();
            const bool reportExists = fs::exists(absolute, error);
            if (error)
                return "Cannot inspect report path '" + absolute.string() + "': " + error.message();
            if (reportExists && fs::is_directory(absolute, error))
                return "The report path names a directory: '" + absolute.string() + "'.";
            if (error)
                return "Cannot inspect report path '" + absolute.string() + "': " + error.message();

            const Path temporary = absolute.parent_path() /
                                   ("." + absolute.filename().string() + ".tmp-" + UuidGenerator::Generate().ToString());
            {
                std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
                if (!stream)
                    return "Cannot write temporary report '" + temporary.string() + "'.";
                stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
                stream.put('\n');
                stream.flush();
                if (!stream)
                {
                    stream.close();
                    fs::remove(temporary, error);
                    return "Writing temporary report failed for '" + temporary.string() + "'.";
                }
            }

#ifdef CW_PLATFORM_WIN32
            if (!MoveFileExW(temporary.wstring().c_str(), absolute.wstring().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                const DWORD replaceError = GetLastError();
                fs::remove(temporary, error);
                return "Cannot publish report '" + absolute.string() + "': Windows error " + std::to_string(replaceError) + ".";
            }
#else
            fs::rename(temporary, absolute, error);
            if (error)
            {
                fs::remove(temporary, error);
                return "Cannot publish report '" + absolute.string() + "': " + error.message();
            }
#endif
            return {};
        }

        void PrintTextReport(std::ostream& output, const BuildPipelineReport& report)
        {
            for (const BuildPipelineStageReport& stage : report.Stages)
            {
                output << '[' << ToString(stage.Status) << "] " << ToString(stage.Stage) << '\n';
                for (const BuildIssue& issue : stage.Diagnostics.Issues)
                {
                    output << "  " << (issue.Severity == BuildIssueSeverity::Warning ? "warning" : "error") << " [" << issue.Code
                           << "] " << issue.Message;
                    if (!issue.Subject.empty())
                        output << " (" << issue.Subject << ')';
                    output << '\n';
                }
            }
            if (report.Succeeded())
                output << "Player build published to " << report.OutputDirectory.string() << '\n';
            else if (report.Cancelled)
                output << "Player build cancelled.\n";
            else
                output << "Player build failed.\n";
            if (!report.Fingerprint.empty())
                output << "Fingerprint: " << report.Fingerprint << '\n';
        }

        BuilderExitCode ExitCodeFor(const BuildPipelineReport& report)
        {
            if (report.Cancelled)
                return BuilderExitCode::Cancelled;
            return report.Succeeded() ? BuilderExitCode::Success : BuilderExitCode::BuildFailed;
        }

        void PrintError(std::ostream& output, std::ostream& errorOutput, BuilderOutputFormat format, BuilderExitCode exitCode,
                        const BuilderError& error)
        {
            if (format == BuilderOutputFormat::Json)
                output << SerializeError(exitCode, error) << '\n';
            else
            {
                errorOutput << "error [" << error.Code << "]: " << error.Message;
                if (!error.Subject.empty())
                    errorOutput << " (" << error.Subject << ')';
                errorOutput << '\n';
            }
        }
    } // namespace

    int RunCrownyBuilder(const Vector<String>& arguments, std::ostream& output, std::ostream& error,
                         BuildCancellationCheck cancellation)
    {
        BuilderCommandLine command;
        BuilderError commandError;
        if (!ParseCommandLine(arguments, command, commandError))
        {
            PrintError(output, error, command.Format, BuilderExitCode::InvalidCommandLine, commandError);
            if (command.Format == BuilderOutputFormat::Text)
                error << '\n' << Usage();
            return static_cast<int>(BuilderExitCode::InvalidCommandLine);
        }
        if (command.ShowHelp)
        {
            output << Usage();
            return static_cast<int>(BuilderExitCode::Success);
        }
        if (command.ShowVersion)
        {
            output << CROWNY_VERSION_STRING << '\n';
            return static_cast<int>(BuilderExitCode::Success);
        }

        try
        {
            BuilderLoadResult loaded = LoadRequest(command.RequestFile);
            if (!loaded.Succeeded())
            {
                const String json = SerializeError(BuilderExitCode::InputError, loaded.Error);
                if (!command.ReportFile.empty())
                {
                    if (const String writeError = WriteReportAtomically(command.ReportFile, json); !writeError.empty())
                    {
                        const BuilderError reportError{ "builder.report.write_failed", writeError, command.ReportFile.string() };
                        PrintError(output, error, command.Format, BuilderExitCode::InternalError, reportError);
                        return static_cast<int>(BuilderExitCode::InternalError);
                    }
                }
                PrintError(output, error, command.Format, BuilderExitCode::InputError, loaded.Error);
                return static_cast<int>(BuilderExitCode::InputError);
            }

            if (command.Format == BuilderOutputFormat::Text)
                output << "Building target " << loaded.Request.Target.Id.ToString() << " from profile '" << loaded.Request.Profile.Name << "'.\n";
            const BuildPipelineReport report = BuildPipeline().Run(std::move(loaded.Request), std::move(cancellation));
            BuilderExitCode exitCode = ExitCodeFor(report);
            const String json = SerializeReport(report, exitCode);
            if (!command.ReportFile.empty())
            {
                if (const String writeError = WriteReportAtomically(command.ReportFile, json); !writeError.empty())
                {
                    const BuilderError reportError{ "builder.report.write_failed", writeError, command.ReportFile.string() };
                    PrintError(output, error, command.Format, BuilderExitCode::InternalError, reportError);
                    return static_cast<int>(BuilderExitCode::InternalError);
                }
            }
            if (command.Format == BuilderOutputFormat::Json)
                output << json << '\n';
            else
                PrintTextReport(output, report);
            return static_cast<int>(exitCode);
        }
        catch (const std::exception& exception)
        {
            const BuilderError internal{ "builder.internal.exception", "Crowny Builder failed: " + String(exception.what()) };
            PrintError(output, error, command.Format, BuilderExitCode::InternalError, internal);
            return static_cast<int>(BuilderExitCode::InternalError);
        }
        catch (...)
        {
            const BuilderError internal{ "builder.internal.exception", "Crowny Builder failed with an unknown exception." };
            PrintError(output, error, command.Format, BuilderExitCode::InternalError, internal);
            return static_cast<int>(BuilderExitCode::InternalError);
        }
    }
} // namespace Crowny
