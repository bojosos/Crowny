#pragma once

namespace Crowny
{
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
        MonoDevelop
	};

	struct ScriptProjectReference
	{
        String Name;
		Path Filepath;
	};

	struct CodeProjectData
	{
        String Name;
        Vector<Path> ScriptFiles;
        Vector<Path> NonScriptFiles;
        String Defines;
        Vector<ScriptProjectReference> AssemblyReferences;
        Vector<ScriptProjectReference> ProjectReferences;
	};

	struct CodeSolutionData
	{
        String Name;
        Vector<CodeProjectData> Projects;
	};

	class CSProject
	{
    public:
        static String GenerateSolution(CSProjectVersion version, const CodeSolutionData& data);
        static String GenerateProject(CSProjectVersion version, const CodeProjectData& projectData);

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

	class ScriptProjectGenerator
	{
    public:
        static void Sync();
	};

}