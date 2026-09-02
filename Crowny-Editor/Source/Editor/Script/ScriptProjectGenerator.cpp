#include "cwepch.h"

#include "Editor/Script/ScriptProjectGenerator.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Utils/Cryptography.h"

#include <algorithm>

namespace Crowny
{
    const String CSProject::SolutionTemplate =
      R"(Microsoft Visual Studio Solution File, Format Version {0}
# Visual Studio {1}{2}{3}
Global
	GlobalSection(SolutionConfigurationPlatforms) = preSolution
		Debug|Any CPU = Debug|Any CPU
		Release|Any CPU = Release|Any CPU
	EndGlobalSection
	GlobalSection(ProjectConfigurationPlatforms) = postSolution{4}
	EndGlobalSection
	GlobalSection(SolutionProperties) = preSolution
		HideSolutionNode = FALSE
	EndGlobalSection
EndGlobal
)";

    const String CSProject::ProjectEntryTemplate = R"(
Project("{{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}}") = "{0}", "{1}", "{{{2}}}"
EndProject)";

    const String CSProject::ProjectPlatformTemplate =
      R"(
		{{{0}}}.Debug|Any CPU.ActiveCfg = Debug|Any CPU
		{{{0}}}.Debug|Any CPU.Build.0 = Debug|Any CPU
		{{{0}}}.Release|Any CPU.ActiveCfg = Release|Any CPU
		{{{0}}}.Release|Any CPU.Build.0 = Release|Any CPU)";

    const String CSProject::ProjectTemplate =
      R"literal(<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="{0}" DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <Import Project="$(MSBuildExtensionsPath)\$(MSBuildToolsVersion)\Microsoft.Common.props" Condition="Exists('$(MSBuildExtensionsPath)\$(MSBuildToolsVersion)\Microsoft.Common.props')" />
  <PropertyGroup>
	<LangVersion>{1}</LangVersion>
  </PropertyGroup>
  <PropertyGroup>
	<Configuration Condition = " '$(Configuration)' == '' ">Debug</Configuration>
	<Platform Condition = " '$(Platform)' == '' ">AnyCPU</Platform>
	<ProjectGuid>{{{2}}}</ProjectGuid>
	<OutputType>Library</OutputType>
	<AppDesignerFolder>Properties</AppDesignerFolder>
	<RootNamespace></RootNamespace>
	<AssemblyName>{3}</AssemblyName>
	<TargetFrameworkVersion>{4}</TargetFrameworkVersion>
	<FileAlignment>512</FileAlignment>
	<BaseDirectory>Resources</BaseDirectory>
	<SchemaVersion>2.0</SchemaVersion>
  </PropertyGroup>
  <PropertyGroup Condition = " '$(Configuration)|$(Platform)' == 'Debug|AnyCPU' ">
	<DebugSymbols>true</DebugSymbols>
	<DebugType>portable</DebugType>
	<Optimize>false</Optimize>
	<OutputPath>Internal\Assemblies\Debug\</OutputPath>
	<BaseIntermediateOutputPath>Internal\Assemblies\</BaseIntermediateOutputPath>
	<DefineConstants>DEBUG;TRACE;{5}</DefineConstants>
	<ErrorReport>prompt</ErrorReport>
	<WarningLevel>4</WarningLevel>
  </PropertyGroup>
  <PropertyGroup Condition = " '$(Configuration)|$(Platform)' == 'Release|AnyCPU' ">
	<DebugType>portable</DebugType>
	<Optimize>true</Optimize>
	<OutputPath>Internal\Assemblies\Release\</OutputPath>
	<BaseIntermediateOutputPath>Internal\Assemblies\</BaseIntermediateOutputPath>
	<DefineConstants>TRACE;{5}</DefineConstants>
	<ErrorReport>prompt</ErrorReport>
	<WarningLevel>4</WarningLevel>
  </PropertyGroup>
  <ItemGroup>{6}
  </ItemGroup>
  <ItemGroup>{7}
  </ItemGroup>
  <ItemGroup>{8}
  </ItemGroup>
  <ItemGroup>{9}
  </ItemGroup>
  <Import Project = "$(MSBuildToolsPath)\Microsoft.CSharp.targets"/>
</Project>)literal";

    const String SdkProjectTemplate =
      R"literal(<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>{0}</TargetFramework>
    <AssemblyName>{1}</AssemblyName>
    <RootNamespace>{1}</RootNamespace>
    <LangVersion>latest</LangVersion>
    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>
    <EnableDefaultNoneItems>false</EnableDefaultNoneItems>
    <Nullable>disable</Nullable>
    <Deterministic>true</Deterministic>
    <GenerateDependencyFile>true</GenerateDependencyFile>
    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>
    <AppendRuntimeIdentifierToOutputPath>false</AppendRuntimeIdentifierToOutputPath>
    <OutputPath>Internal/Assemblies/$(Configuration)/</OutputPath>
    <BaseIntermediateOutputPath>Internal/Assemblies/obj/</BaseIntermediateOutputPath>
    <DefineConstants>$(DefineConstants);{2}</DefineConstants>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)' == 'Debug'">
    <Optimize>false</Optimize>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)' == 'Release'">
    <Optimize>true</Optimize>
  </PropertyGroup>
  <ItemGroup>{3}
  </ItemGroup>
  <ItemGroup>{4}
  </ItemGroup>
  <ItemGroup>{5}
  </ItemGroup>
  <ItemGroup>{6}
  </ItemGroup>
</Project>)literal";

    const String CSProject::ReferenceEntryTemplate =
      R"(
	<Reference Include="{0}"/>)";

    const String CSProject::ReferencePathEntryTemplate =
      R"(
	<Reference Include="{0}">
	  <HintPath>{1}</HintPath>
	</Reference>)";

    const String CSProject::ReferenceProjectEntryTemplate =
      R"(
	<ProjectReference Include="{2}">
	  <Project>{{{1}}}</Project>
	  <Name>{0}</Name>
	</ProjectReference>)";

    const String CSProject::ScriptEntryTemplate =
      R"(
	<Compile Include="{0}"/>)";

    const String CSProject::NonScriptEntryTemplate =
      R"(
	<None Include="{0}"/>)";

    String GetProjectGUID(const String& projectName);

    namespace
    {
        struct ProjectVersionInfo
        {
            const char* SolutionFormat;
            const char* VisualStudioName;
            const char* VisualStudioVersion;
            const char* MinimumVisualStudioVersion;
            const char* ToolsVersion;
            const char* LegacyLanguageVersion;
        };

        const ProjectVersionInfo& GetProjectVersionInfo(CSProjectVersion version)
        {
            static const ProjectVersionInfo vs2008 = { "10.00", "2008", "", "", "3.5", "3.0" };
            static const ProjectVersionInfo vs2010 = { "11.00", "2010", "10.0.40219.1", "10.0.40219.1", "4.0", "4.0" };
            static const ProjectVersionInfo vs2012 = { "12.00", "2012", "11.0.50727.1", "10.0.40219.1", "4.0", "5.0" };
            static const ProjectVersionInfo vs2013 = { "12.00", "2013", "12.0.21005.1", "10.0.40219.1", "12.0", "5.0" };
            static const ProjectVersionInfo vs2015 = { "12.00", "14", "14.0.23107.0", "10.0.40219.1", "14.0", "6.0" };
            static const ProjectVersionInfo vs2017 = { "12.00", "15", "15.0.26730.16", "10.0.40219.1", "15.0", "7.3" };
            static const ProjectVersionInfo vs2019 = { "12.00", "Version 16", "16.0.28701.123", "10.0.40219.1", "16.0", "8.0" };
            static const ProjectVersionInfo vs2022 = { "12.00", "Version 17", "17.0.31903.59", "10.0.40219.1", "17.0", "9.0" };
            static const ProjectVersionInfo vs2026 = { "12.00", "Version 18", "18.0.11205.157", "10.0.40219.1", "Current", "9.0" };
            static const ProjectVersionInfo monoDevelop = { "12.00", "MonoDevelop", "", "", "14.0", "7.3" };

            switch (version)
            {
            case CSProjectVersion::VS2008:
                return vs2008;
            case CSProjectVersion::VS2010:
                return vs2010;
            case CSProjectVersion::VS2012:
                return vs2012;
            case CSProjectVersion::VS2013:
                return vs2013;
            case CSProjectVersion::VS2015:
                return vs2015;
            case CSProjectVersion::VS2017:
                return vs2017;
            case CSProjectVersion::VS2019:
                return vs2019;
            case CSProjectVersion::VS2022:
                return vs2022;
            case CSProjectVersion::VS2026:
                return vs2026;
            case CSProjectVersion::MonoDevelop:
                return monoDevelop;
            }

            CW_ENGINE_ASSERT(false, "Unsupported C# project version.");
            return vs2022;
        }

        String EscapeXml(String value)
        {
            value = StringUtils::Replace(value, "&", "&amp;");
            value = StringUtils::Replace(value, "\"", "&quot;");
            value = StringUtils::Replace(value, "'", "&apos;");
            value = StringUtils::Replace(value, "<", "&lt;");
            return StringUtils::Replace(value, ">", "&gt;");
        }

        String EscapeSolutionValue(String value) { return StringUtils::Replace(value, "\"", "\"\""); }

        String MakeProjectPath(const Path& path, const Path& projectDirectory)
        {
            Path formattedPath = path.lexically_normal();
            if (!projectDirectory.empty() && formattedPath.is_absolute())
            {
                const Path relativePath = formattedPath.lexically_relative(projectDirectory.lexically_normal());
                if (!relativePath.empty())
                    formattedPath = relativePath;
            }
            return formattedPath.generic_string();
        }

        Vector<Path> SortedPaths(const Vector<Path>& paths)
        {
            Vector<Path> result = paths;
            std::sort(result.begin(), result.end(), [](const Path& lhs, const Path& rhs) { return lhs.generic_string() < rhs.generic_string(); });
            result.erase(std::unique(result.begin(), result.end(),
                                     [](const Path& lhs, const Path& rhs) { return lhs.lexically_normal() == rhs.lexically_normal(); }),
                         result.end());
            return result;
        }

        Vector<ScriptProjectReference> SortedReferences(const Vector<ScriptProjectReference>& references)
        {
            Vector<ScriptProjectReference> result = references;
            std::sort(result.begin(), result.end(), [](const ScriptProjectReference& lhs, const ScriptProjectReference& rhs) {
                if (lhs.Name != rhs.Name)
                    return lhs.Name < rhs.Name;
                return lhs.Filepath.generic_string() < rhs.Filepath.generic_string();
            });
            result.erase(std::unique(result.begin(), result.end(),
                                     [](const ScriptProjectReference& lhs, const ScriptProjectReference& rhs) {
                                         return lhs.Name == rhs.Name && lhs.Filepath.lexically_normal() == rhs.Filepath.lexically_normal();
                                     }),
                         result.end());
            return result;
        }

        String GenerateReferences(const CodeProjectData& projectData, const String& referenceEntryTemplate, const String& referencePathEntryTemplate)
        {
            StringStream entries;
            for (const ScriptProjectReference& reference : SortedReferences(projectData.AssemblyReferences))
            {
                const String name = EscapeXml(reference.Name);
                if (reference.Filepath.empty())
                    entries << fmt::format(fmt::runtime(referenceEntryTemplate), name);
                else
                    entries << fmt::format(fmt::runtime(referencePathEntryTemplate), name,
                                           EscapeXml(MakeProjectPath(reference.Filepath, projectData.ProjectDirectory)));
            }
            return entries.str();
        }

        String GenerateProjectReferences(const CodeProjectData& projectData, const String& legacyProjectReferenceTemplate)
        {
            StringStream entries;
            for (const ScriptProjectReference& reference : SortedReferences(projectData.ProjectReferences))
            {
                const Path path = reference.Filepath.empty() ? Path(reference.Name + ".csproj") : reference.Filepath;
                const String include = EscapeXml(MakeProjectPath(path, projectData.ProjectDirectory));
                if (projectData.Runtime == CSharpProjectRuntime::CoreCLR)
                    entries << fmt::format(fmt::runtime("\n    <ProjectReference Include=\"{0}\" />"), include);
                else
                    entries << fmt::format(fmt::runtime(legacyProjectReferenceTemplate), EscapeXml(reference.Name), GetProjectGUID(reference.Name),
                                           include);
            }
            return entries.str();
        }

        String GenerateFiles(const Vector<Path>& paths, const Path& projectDirectory, const String& entryTemplate)
        {
            StringStream entries;
            for (const Path& path : SortedPaths(paths))
                entries << fmt::format(fmt::runtime(entryTemplate), EscapeXml(MakeProjectPath(path, projectDirectory)));
            return entries.str();
        }

        bool WriteGeneratedTextFile(const Path& path, const String& contents, bool& changed)
        {
            std::error_code error;
            if (fs::is_regular_file(path, error))
            {
                const Ref<DataStream> input = FileSystem::OpenFile(path);
                if (input != nullptr && input->GetAsString() == contents)
                    return true;
            }

            String writeError;
            if (!FileSystem::WriteTextFileAtomic(path, contents, &writeError))
            {
                CW_ENGINE_ERROR("Could not write generated project file {}: {}", path.string(), writeError);
                return false;
            }

            changed = true;
            return true;
        }

        struct SolutionProjectBlock
        {
            String Text;
            String Guid;
        };

        String NormalizeSolutionGuid(String guid)
        {
            StringUtils::ToUpper(guid);
            return guid;
        }

        Vector<SolutionProjectBlock> FindSolutionProjectBlocks(const String& solution)
        {
            Vector<SolutionProjectBlock> blocks;
            size_t start = 0;
            while ((start = solution.find("Project(", start)) != String::npos)
            {
                const size_t endMarker = solution.find("\nEndProject", start);
                if (endMarker == String::npos)
                    break;

                size_t end = endMarker + String("\nEndProject").size();
                if (end < solution.size() && solution[end] == '\n')
                    ++end;
                String block = solution.substr(start, end - start);
                const size_t guidStart = block.rfind("\"{");
                const size_t guidEnd = guidStart == String::npos ? String::npos : block.find("}\"", guidStart + 2);
                const String guid = guidStart == String::npos || guidEnd == String::npos
                                      ? String()
                                      : NormalizeSolutionGuid(block.substr(guidStart + 2, guidEnd - guidStart - 2));
                blocks.push_back({ std::move(block), guid });
                start = end;
            }
            return blocks;
        }

        String FindGlobalSectionEntries(const String& solution, const String& sectionName)
        {
            const String sectionMarker = "GlobalSection(" + sectionName + ")";
            const size_t sectionStart = solution.find(sectionMarker);
            if (sectionStart == String::npos)
                return {};

            const size_t entriesStart = solution.find('\n', sectionStart);
            if (entriesStart == String::npos)
                return {};
            const size_t sectionEnd = solution.find("\n\tEndGlobalSection", entriesStart);
            if (sectionEnd == String::npos)
                return {};
            return solution.substr(entriesStart + 1, sectionEnd - entriesStart - 1);
        }

        void AppendGlobalSectionEntries(String& solution, const String& sectionName, const String& entries)
        {
            if (entries.empty())
                return;
            const String sectionMarker = "GlobalSection(" + sectionName + ")";
            const size_t sectionStart = solution.find(sectionMarker);
            if (sectionStart == String::npos)
                return;
            const size_t sectionEnd = solution.find("\n\tEndGlobalSection", sectionStart);
            if (sectionEnd == String::npos)
                return;

            solution.insert(sectionEnd, "\n" + entries);
        }

        String FilterProjectConfigurationEntries(const String& entries, const Set<String>& projectGuids)
        {
            StringStream input(entries);
            StringStream output;
            String line;
            while (std::getline(input, line))
            {
                const size_t guidStart = line.find('{');
                const size_t guidEnd = guidStart == String::npos ? String::npos : line.find('}', guidStart + 1);
                if (guidStart != String::npos && guidEnd != String::npos &&
                    projectGuids.contains(NormalizeSolutionGuid(line.substr(guidStart + 1, guidEnd - guidStart - 1))))
                    output << line << '\n';
            }
            return output.str();
        }

        String MergeSolution(const String& previousSolution, const String& generatedSolution, const CodeSolutionData& data)
        {
            const String normalizedPrevious = StringUtils::Replace(previousSolution, "\r\n", "\n");
            Set<String> generatedGuids;
            for (const CodeProjectData& project : data.Projects)
                generatedGuids.insert(GetProjectGUID(project.Name));

            String merged = generatedSolution;
            Set<String> externalGuids;
            StringStream externalProjects;
            for (const SolutionProjectBlock& block : FindSolutionProjectBlocks(normalizedPrevious))
            {
                if (block.Guid.empty() || generatedGuids.contains(block.Guid))
                    continue;
                externalProjects << '\n' << block.Text;
                externalGuids.insert(block.Guid);
            }

            const size_t globalStart = merged.find("\nGlobal\n");
            if (globalStart != String::npos)
                merged.insert(globalStart, externalProjects.str());

            AppendGlobalSectionEntries(
              merged, "ProjectConfigurationPlatforms",
              FilterProjectConfigurationEntries(FindGlobalSectionEntries(normalizedPrevious, "ProjectConfigurationPlatforms"), externalGuids));

            const String nestedProjects = FindGlobalSectionEntries(normalizedPrevious, "NestedProjects");
            if (!nestedProjects.empty())
            {
                const size_t globalEnd = merged.find("\nEndGlobal\n");
                if (globalEnd != String::npos)
                    merged.insert(globalEnd, "\n\tGlobalSection(NestedProjects) = preSolution\n" + nestedProjects + "\n\tEndGlobalSection");
            }

            return merged;
        }
    } // namespace

    String GetProjectGUID(const String& projectName)
    {
        static const String guidTemplate = "{0}-{1}-{2}-{3}-{4}";
        String hash = Cryptography::MD5(projectName);
        String result =
          fmt::format(fmt::runtime(guidTemplate), hash.substr(0, 8), hash.substr(8, 4), hash.substr(12, 4), hash.substr(16, 4), hash.substr(20, 12));
        StringUtils::ToUpper(result);
        return result;
    }

    String CSProject::GenerateSolution(CSProjectVersion version, const CodeSolutionData& data)
    {
        const ProjectVersionInfo& versionInfo = GetProjectVersionInfo(version);

        StringStream projectEntriesStream;
        StringStream projectPlatformsStream;
        for (const CodeProjectData& project : data.Projects)
        {
            const String guid = GetProjectGUID(project.Name);
            projectEntriesStream << fmt::format(fmt::runtime(ProjectEntryTemplate), EscapeSolutionValue(project.Name),
                                                EscapeSolutionValue(project.Name + ".csproj"), guid);
            projectPlatformsStream << fmt::format(fmt::runtime(ProjectPlatformTemplate), guid);
        }

        String versionMetadata;
        if (versionInfo.VisualStudioVersion[0] != '\0')
        {
            versionMetadata = fmt::format("\nVisualStudioVersion = {}\nMinimumVisualStudioVersion = {}", versionInfo.VisualStudioVersion,
                                          versionInfo.MinimumVisualStudioVersion);
        }
        return fmt::format(fmt::runtime(SolutionTemplate), versionInfo.SolutionFormat, versionInfo.VisualStudioName, versionMetadata,
                           projectEntriesStream.str(), projectPlatformsStream.str());
    }

    String CSProject::GenerateProject(CSProjectVersion projectVersion, const CodeProjectData& projectData)
    {
        const ProjectVersionInfo& versionInfo = GetProjectVersionInfo(projectVersion);
        const String scriptEntries = GenerateFiles(projectData.ScriptFiles, projectData.ProjectDirectory, ScriptEntryTemplate);
        const String nonScriptEntries = GenerateFiles(projectData.NonScriptFiles, projectData.ProjectDirectory, NonScriptEntryTemplate);
        const String referenceEntries = GenerateReferences(projectData, ReferenceEntryTemplate, ReferencePathEntryTemplate);
        const String projectReferenceEntries = GenerateProjectReferences(projectData, ReferenceProjectEntryTemplate);
        const String projectGUID = GetProjectGUID(projectData.Name);
        const String name = EscapeXml(projectData.Name);
        const String defines = EscapeXml(projectData.Defines);

        if (projectData.Runtime == CSharpProjectRuntime::CoreCLR)
        {
            const String targetFramework = projectData.TargetFramework.empty() ? "net10.0" : EscapeXml(projectData.TargetFramework);
            return fmt::format(fmt::runtime(SdkProjectTemplate), targetFramework, name, defines, referenceEntries, projectReferenceEntries,
                               scriptEntries, nonScriptEntries);
        }

        return fmt::format(fmt::runtime(ProjectTemplate), versionInfo.ToolsVersion, versionInfo.LegacyLanguageVersion, projectGUID, name, "v4.7.2",
                           defines, referenceEntries, projectReferenceEntries, scriptEntries, nonScriptEntries);
    }

    bool CSProject::WriteSolution(CSProjectVersion version, const CodeSolutionData& data, const Path& solutionDirectory, bool* outChanged)
    {
        bool changed = false;
        for (const CodeProjectData& project : data.Projects)
        {
            String projectText = GenerateProject(version, project);
            projectText = StringUtils::Replace(projectText, "\n", "\r\n");

            const Path projectPath = solutionDirectory / (project.Name + ".csproj");
            if (!WriteGeneratedTextFile(projectPath, projectText, changed))
            {
                return false;
            }
        }

        String solutionText = GenerateSolution(version, data);
        const Path solutionPath = solutionDirectory / (data.Name + ".sln");
        std::error_code readError;
        if (fs::is_regular_file(solutionPath, readError))
            solutionText = MergeSolution(FileSystem::ReadTextFile(solutionPath), solutionText, data);
        solutionText = StringUtils::Replace(solutionText, "\n", "\r\n");

        if (!WriteGeneratedTextFile(solutionPath, solutionText, changed))
        {
            return false;
        }

        if (outChanged != nullptr)
            *outChanged = changed;
        return true;
    }
} // namespace Crowny
