#pragma once

namespace Crowny
{
    struct ScriptProjectReference
    {
        String Name;
        Path Filepath;
    };

    enum class CSharpProjectRuntime
    {
        Mono,
        CoreCLR
    };

    struct CodeProjectData
    {
        String Name;
        Path ProjectDirectory;
        Vector<Path> ScriptFiles;
        Vector<Path> NonScriptFiles;
        String Defines;
        Vector<ScriptProjectReference> AssemblyReferences;
        Vector<ScriptProjectReference> ProjectReferences;
        CSharpProjectRuntime Runtime = CSharpProjectRuntime::Mono;
        String TargetFramework = "net10.0";
    };

    struct CodeSolutionData
    {
        String Name;
        Vector<CodeProjectData> Projects;
    };

    enum class CSProjectVersion
    {
        VS2008,
        VS2010,
        VS2012,
        VS2013,
        VS2015,
        VS2017,
        VS2019,
        VS2022,
        VS2026,
        MonoDevelop
    };

    class CSProject
    {
    public:
        static String GenerateSolution(CSProjectVersion version, const CodeSolutionData& data);
        static String GenerateProject(CSProjectVersion version, const CodeProjectData& projectData);
        static bool WriteSolution(CSProjectVersion version, const CodeSolutionData& data, const Path& solutionDirectory, bool* outChanged = nullptr);

    private:
        static const String SolutionTemplate;
        static const String ProjectEntryTemplate;
        static const String ProjectPlatformTemplate;
        static const String ProjectTemplate;
        static const String ReferenceEntryTemplate;
        static const String ReferenceProjectEntryTemplate;
        static const String ReferencePathEntryTemplate;
        static const String ScriptEntryTemplate;
        static const String NonScriptEntryTemplate;
    };

} // namespace Crowny
