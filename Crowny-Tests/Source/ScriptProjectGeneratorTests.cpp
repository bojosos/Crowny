#include <catch2/catch_test_macros.hpp>

#include "Crowny/Common/FileSystem.h"
#include "Editor/Script/ScriptProjectGenerator.h"
#include "Editor/Script/VSCodeEditor.h"

using namespace Crowny;

namespace
{
    CodeProjectData MakeProject(CSharpProjectRuntime runtime = CSharpProjectRuntime::Mono)
    {
        CodeProjectData project;
        project.Name = "Game&Assembly";
        project.ProjectDirectory = "C:/Projects/Crowny Game";
        project.Runtime = runtime;
        project.Defines = "CROWNY_WIN;CROWNY_64";
        project.ScriptFiles = {
            "C:/Projects/Crowny Game/Assets/Gameplay/Zeta.cs",
            "C:/Projects/Crowny Game/Assets/Gameplay/A & B.cs",
        };
        project.NonScriptFiles = { "C:/Projects/Crowny Game/Assets/Readme & Notes.txt" };
        project.AssemblyReferences = { { "Crowny&Sharp", "C:/Shared/Crowny&Sharp.dll" } };
        project.ProjectReferences = { { "Shared&Tools", "C:/Projects/Crowny Game/Libraries/Shared & Tools.csproj" } };
        return project;
    }
} // namespace

TEST_CASE("Legacy C# project generation uses the selected Visual Studio toolset", "[Editor][Scripting][Projects]")
{
    struct VersionExpectation
    {
        CSProjectVersion Version;
        String ToolsVersion;
        String LanguageVersion;
    };
    const Vector<VersionExpectation> versions = {
        { CSProjectVersion::VS2008, "3.5", "3.0" },  { CSProjectVersion::VS2010, "4.0", "4.0" },  { CSProjectVersion::VS2012, "4.0", "5.0" },
        { CSProjectVersion::VS2013, "12.0", "5.0" }, { CSProjectVersion::VS2015, "14.0", "6.0" }, { CSProjectVersion::VS2017, "15.0", "7.3" },
        { CSProjectVersion::VS2019, "16.0", "8.0" }, { CSProjectVersion::VS2022, "17.0", "9.0" }, { CSProjectVersion::VS2026, "Current", "9.0" },
    };
    for (const VersionExpectation& expected : versions)
    {
        CAPTURE(expected.ToolsVersion, expected.LanguageVersion);
        const String generated = CSProject::GenerateProject(expected.Version, MakeProject());
        CHECK(generated.find("ToolsVersion=\"" + expected.ToolsVersion + "\"") != String::npos);
        CHECK(generated.find("<LangVersion>" + expected.LanguageVersion + "</LangVersion>") != String::npos);
    }

    const String project = CSProject::GenerateProject(CSProjectVersion::VS2022, MakeProject());
    CHECK(project.find("<TargetFrameworkVersion>v4.7.2</TargetFrameworkVersion>") != String::npos);
    CHECK(project.find("Compile Include=\"Assets/Gameplay/A &amp; B.cs\"") != String::npos);
    CHECK(project.find("None Include=\"Assets/Readme &amp; Notes.txt\"") != String::npos);
    CHECK(project.find("Reference Include=\"Crowny&amp;Sharp\"") != String::npos);
    CHECK(project.find("<HintPath>../../Shared/Crowny&amp;Sharp.dll</HintPath>") != String::npos);
    CHECK(project.find("ProjectReference Include=\"Libraries/Shared &amp; Tools.csproj\"") != String::npos);
    CHECK(project.find("Assets/Gameplay/A &amp; B.cs") < project.find("Assets/Gameplay/Zeta.cs"));
}

TEST_CASE("CoreCLR C# project generation uses an SDK-style project", "[Editor][Scripting][Projects]")
{
    CodeProjectData projectData = MakeProject(CSharpProjectRuntime::CoreCLR);
    const String project = CSProject::GenerateProject(CSProjectVersion::VS2022, projectData);

    CHECK(project.find("<Project Sdk=\"Microsoft.NET.Sdk\">") != String::npos);
    CHECK(project.find("<TargetFramework>net10.0</TargetFramework>") != String::npos);
    CHECK(project.find("<EnableDefaultCompileItems>false</EnableDefaultCompileItems>") != String::npos);
    CHECK(project.find("<EnableDefaultNoneItems>false</EnableDefaultNoneItems>") != String::npos);
    CHECK(project.find("<TargetFrameworkVersion>") == String::npos);
    CHECK(project.find("ProjectReference Include=\"Libraries/Shared &amp; Tools.csproj\"") != String::npos);
}

TEST_CASE("Visual Studio solution generation writes version-correct headers", "[Editor][Scripting][Projects]")
{
    CodeSolutionData solution;
    solution.Name = "Game";
    solution.Projects.push_back(MakeProject());

    struct VersionExpectation
    {
        CSProjectVersion Version;
        String Format;
        String HeaderVersion;
        String VisualStudioVersion;
    };
    const Vector<VersionExpectation> versions = {
        { CSProjectVersion::VS2008, "10.00", "2008", "" },
        { CSProjectVersion::VS2010, "11.00", "2010", "10.0.40219.1" },
        { CSProjectVersion::VS2012, "12.00", "2012", "11.0.50727.1" },
        { CSProjectVersion::VS2013, "12.00", "2013", "12.0.21005.1" },
        { CSProjectVersion::VS2015, "12.00", "14", "14.0.23107.0" },
        { CSProjectVersion::VS2017, "12.00", "15", "15.0.26730.16" },
        { CSProjectVersion::VS2019, "12.00", "Version 16", "16.0.28701.123" },
        { CSProjectVersion::VS2022, "12.00", "Version 17", "17.0.31903.59" },
        { CSProjectVersion::VS2026, "12.00", "Version 18", "18.0.11205.157" },
    };

    for (const VersionExpectation& expected : versions)
    {
        CAPTURE(expected.HeaderVersion);
        const String generated = CSProject::GenerateSolution(expected.Version, solution);
        CHECK(generated.find("Format Version " + expected.Format) != String::npos);
        CHECK(generated.find("# Visual Studio " + expected.HeaderVersion) != String::npos);
        if (!expected.VisualStudioVersion.empty())
            CHECK(generated.find("VisualStudioVersion = " + expected.VisualStudioVersion) != String::npos);
        CHECK(generated.find("Project(\"{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}\")") != String::npos);
    }
}

TEST_CASE("C# solution writer preserves unchanged generated files", "[Editor][Scripting][Projects]")
{
    const Path outputDirectory = fs::temp_directory_path() / "crowny-script-project-generator-writer-tests";
    std::error_code error;
    fs::remove_all(outputDirectory, error);
    REQUIRE(!error);
    fs::create_directories(outputDirectory, error);
    REQUIRE(!error);

    CodeSolutionData solution;
    solution.Name = "Game";
    solution.Projects.push_back(MakeProject());

    bool changed = false;
    REQUIRE(CSProject::WriteSolution(CSProjectVersion::VS2026, solution, outputDirectory, &changed));
    CHECK(changed);

    const Path solutionPath = outputDirectory / "Game.sln";
    CHECK(FileSystem::ReadTextFile(solutionPath).find("\r\n") != String::npos);

    changed = true;
    REQUIRE(CSProject::WriteSolution(CSProjectVersion::VS2026, solution, outputDirectory, &changed));
    CHECK_FALSE(changed);

    fs::remove_all(outputDirectory, error);
    CHECK(!error);
}

TEST_CASE("C# solution writer preserves external solution projects and folders", "[Editor][Scripting][Projects]")
{
    const Path outputDirectory = fs::temp_directory_path() / "crowny-script-solution-merge-tests";
    std::error_code error;
    fs::remove_all(outputDirectory, error);
    REQUIRE(!error);
    fs::create_directories(outputDirectory, error);
    REQUIRE(!error);

    CodeSolutionData solution;
    solution.Name = "Game";
    solution.Projects.push_back(MakeProject());

    const Path solutionPath = outputDirectory / "Game.sln";
    REQUIRE(CSProject::WriteSolution(CSProjectVersion::VS2026, solution, outputDirectory));

    constexpr const char* externalProjectGuid = "A8E345D3-1AC4-4B88-AF71-9C1E4A5C6922";
    constexpr const char* solutionFolderGuid = "0D9A9CB4-0E3C-4D86-861B-210328DECEAF";
    String customized = FileSystem::ReadTextFile(solutionPath);
    const size_t globalStart = customized.find("\r\nGlobal\r\n");
    REQUIRE(globalStart != String::npos);
    const String externalProjects = "\r\nProject(\"{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}\") = \"External.Tools\", \"External/Tools.csproj\", \"{" +
                                    String(externalProjectGuid) +
                                    "}\"\r\nEndProject\r\n"
                                    "Project(\"{66A26720-8FB5-11D2-AA7E-00C04F688DDE}\") = \"External\", \"External\", \"{" +
                                    String(solutionFolderGuid) + "}\"\r\nEndProject\r\n";
    customized.insert(globalStart, externalProjects);

    const size_t globalEnd = customized.find("\r\nEndGlobal\r\n");
    REQUIRE(globalEnd != String::npos);
    customized.insert(globalEnd, "\r\n\tGlobalSection(NestedProjects) = preSolution\r\n\t\t{" + String(externalProjectGuid) + "} = {" +
                                   String(solutionFolderGuid) + "}\r\n\tEndGlobalSection\r\n");

    const String projectConfiguration = "\r\n\t\t{" + String(externalProjectGuid) + "}.Debug|Any CPU.ActiveCfg = Debug|Any CPU";
    const size_t configurationsEnd = customized.find("\r\n\tEndGlobalSection", customized.find("GlobalSection(ProjectConfigurationPlatforms)"));
    REQUIRE(configurationsEnd != String::npos);
    customized.insert(configurationsEnd, projectConfiguration);
    REQUIRE(FileSystem::WriteTextFile(solutionPath, customized));

    REQUIRE(CSProject::WriteSolution(CSProjectVersion::VS2026, solution, outputDirectory));
    const String merged = FileSystem::ReadTextFile(solutionPath);
    CHECK(merged.find("External/Tools.csproj") != String::npos);
    CHECK(merged.find("GlobalSection(NestedProjects)") != String::npos);
    CHECK(merged.find(externalProjectGuid) != String::npos);

    fs::remove_all(outputDirectory, error);
    CHECK(!error);
}

TEST_CASE("VS Code synchronization configures a CoreCLR workspace", "[Editor][Scripting][Projects]")
{
    const Path outputDirectory = fs::temp_directory_path() / "crowny-vscode-project-generator-tests";
    std::error_code error;
    fs::remove_all(outputDirectory, error);
    REQUIRE(!error);
    fs::create_directories(outputDirectory, error);
    REQUIRE(!error);

    CodeSolutionData solution;
    solution.Name = "Game";
    solution.Projects.push_back(MakeProject(CSharpProjectRuntime::CoreCLR));

    VSCodeEditor editor(Path{});
    const CodeEditorSyncResult firstSync = editor.Sync(solution, outputDirectory);
    REQUIRE(firstSync.Succeeded);
    CHECK(firstSync.Changed);

    const Path vscodeDirectory = outputDirectory / ".vscode";
    const String settings = FileSystem::ReadTextFile(vscodeDirectory / "settings.json");
    const String extensions = FileSystem::ReadTextFile(vscodeDirectory / "extensions.json");
    const String launch = FileSystem::ReadTextFile(vscodeDirectory / "launch.json");
    CHECK(settings.find("dotnet.defaultSolution") != String::npos);
    CHECK(settings.find("Game.sln") != String::npos);
    CHECK(extensions.find("ms-dotnettools.csharp") != String::npos);
    CHECK(launch.find("Attach to Crowny (.NET)") != String::npos);

    const CodeEditorSyncResult secondSync = editor.Sync(solution, outputDirectory);
    REQUIRE(secondSync.Succeeded);
    CHECK_FALSE(secondSync.Changed);

    fs::remove_all(outputDirectory, error);
    CHECK(!error);
}

TEST_CASE("VS Code synchronization preserves user workspace settings", "[Editor][Scripting][Projects]")
{
    const Path outputDirectory = fs::temp_directory_path() / "crowny-vscode-project-merge-tests";
    std::error_code error;
    fs::remove_all(outputDirectory, error);
    REQUIRE(!error);
    fs::create_directories(outputDirectory / ".vscode", error);
    REQUIRE(!error);

    REQUIRE(FileSystem::WriteTextFile(outputDirectory / ".vscode" / "settings.json", R"({ "editor.tabSize": 8 })"));
    REQUIRE(FileSystem::WriteTextFile(outputDirectory / ".vscode" / "extensions.json", R"({ "unwantedRecommendations": ["example.extension"] })"));
    REQUIRE(FileSystem::WriteTextFile(outputDirectory / ".vscode" / "launch.json",
                                      R"({ "configurations": [{ "name": "Custom", "type": "coreclr", "request": "launch" }] })"));

    CodeSolutionData solution;
    solution.Name = "Game";
    solution.Projects.push_back(MakeProject(CSharpProjectRuntime::CoreCLR));

    VSCodeEditor editor(Path{});
    const CodeEditorSyncResult sync = editor.Sync(solution, outputDirectory);
    REQUIRE(sync.Succeeded);
    CHECK(sync.Changed);

    const Path vscodeDirectory = outputDirectory / ".vscode";
    const String settings = FileSystem::ReadTextFile(vscodeDirectory / "settings.json");
    const String extensions = FileSystem::ReadTextFile(vscodeDirectory / "extensions.json");
    const String launch = FileSystem::ReadTextFile(vscodeDirectory / "launch.json");
    CHECK(settings.find("editor.tabSize") != String::npos);
    CHECK(extensions.find("unwantedRecommendations") != String::npos);
    CHECK(launch.find("\"Custom\"") != String::npos);
    CHECK(launch.find("Attach to Crowny (.NET)") != String::npos);

    fs::remove_all(outputDirectory, error);
    CHECK(!error);
}
