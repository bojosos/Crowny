#include "cwepch.h"

#include "Editor/Script/ScriptProjectGenerator.h"

#include "Crowny/Common/StringUtils.h"
#include "Crowny/Utils/Cryptography.h"

namespace Crowny
{
    const String CSProject::SolutionTemplate =
      R"(Microsoft Visual Studio Solution File, Format Version {0}
# Visual Studio 2019
VisualStudioVersion = 12.0.30723.0
MinimumVisualStudioVersion = 10.0.40219.1{1}
Global
	GlobalSection(SolutionConfigurationPlatforms) = preSolution
		Debug|Any CPU = Debug|Any CPU
		Release|Any CPU = Release|Any CPU
	EndGlobalSection
	GlobalSection(ProjectConfigurationPlatforms) = postSolution{2}
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
  <Import Project="$(MSBuildExtensionsPath)\\$(MSBuildToolsVersion)\\Microsoft.Common.props" Condition="Exists('$(MSBuildExtensionsPath)\\$(MSBuildToolsVersion)\\Microsoft.Common.props')" />
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
	<TargetFrameworkVersion>v4.8</TargetFrameworkVersion>
	<FileAlignment>512</FileAlignment>
	<BaseDirectory>Resources</BaseDirectory>
	<SchemaVersion>2.0</SchemaVersion>
  </PropertyGroup>
  <PropertyGroup Condition = " '$(Configuration)|$(Platform)' == 'Debug|AnyCPU' ">
	<DebugSymbols>true</DebugSymbols>
	<DebugType>full</DebugType>
	<Optimize>false</Optimize>
	<OutputPath>Internal\\Assemblies\\Debug\\</OutputPath>
	<BaseIntermediateOutputPath>Internal\\Assemblies\\</BaseIntermediateOutputPath>
	<DefineConstants>DEBUG;TRACE;{4}</DefineConstants>
	<ErrorReport>prompt</ErrorReport>
	<WarningLevel>4</WarningLevel >
  </PropertyGroup>
  <PropertyGroup Condition = " '$(Configuration)|$(Platform)' == 'Release|AnyCPU' ">
	<DebugType>pdbonly</DebugType>
	<Optimize>true</Optimize>
	<OutputPath>Internal\\Assemblies\\Release\\</OutputPath>
	<BaseIntermediateOutputPath>Internal\\Assemblies\\</BaseIntermediateOutputPath>
	<DefineConstants>TRACE;{4}</DefineConstants>
	<ErrorReport>prompt</ErrorReport>
	<WarningLevel>4</WarningLevel>
  </PropertyGroup>
  <ItemGroup>{5}
  </ItemGroup>
  <ItemGroup>{6}
  </ItemGroup>
  <ItemGroup>{7}
  </ItemGroup>
  <ItemGroup>{8}
  </ItemGroup>
  <Import Project = "$(MSBuildToolsPath)\\Microsoft.CSharp.targets"/>
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
	<ProjectReference Include="{0}.csproj">
	  <Project>{{{1}}}</Project>
	  <Name>{0}</Name>
	</ProjectReference>)";

    const String CSProject::ScriptEntryTemplate =
      R"(
	<Compile Include="{0}"/>)";

    const String CSProject::NonScriptEntryTemplate =
      R"(
	<None Include="{0}"/>)";

    String GetProjectGUID(const String& projectName)
    {
        static const String guidTemplate = "{0}-{1}-{2}-{3}-{4}";
        String hash = Cryptography::MD5(projectName);
        String result = fmt::format(guidTemplate, hash.substr(0, 8), hash.substr(8, 4), hash.substr(12, 4),
                                            hash.substr(16, 4), hash.substr(20, 12));
        StringUtils::ToUpper(result);
        return result;
    }

    String CSProject::GenerateSolution(CSProjectVersion version, const CodeSolutionData& data)
    {
        Map<CSProjectVersion, String> fileFormatData = {
            { CSProjectVersion::VS2008, "10.0" }, { CSProjectVersion::VS2010, "11.0" },
            { CSProjectVersion::VS2012, "12.0" }, { CSProjectVersion::VS2013, "12.0" },
            { CSProjectVersion::VS2015, "12.0" }, { CSProjectVersion::VS2017, "12.0" },
            { CSProjectVersion::VS2019, "12.0" }, { CSProjectVersion::MonoDevelop, "12.0" },
        };

        StringStream projectEntriesStream;
        StringStream projectPlatformsStream;
        for (const CodeProjectData& project : data.Projects)
        {
            String guid = GetProjectGUID(project.Name);
            projectEntriesStream << fmt::format(ProjectEntryTemplate, project.Name, project.Name + ".csproj", guid);
            projectPlatformsStream << fmt::format(ProjectPlatformTemplate, guid);
        }

        return fmt::format(SolutionTemplate, fileFormatData[version], projectEntriesStream.str(),
                                   projectPlatformsStream.str());
    }

    String CSProject::GenerateProject(CSProjectVersion projectVersion, const CodeProjectData& projectData)
    {
        Map<CSProjectVersion, String> versionData = {
            { CSProjectVersion::VS2008, "3.5" }, { CSProjectVersion::VS2010, "4.0" },
            { CSProjectVersion::VS2012, "4.0" }, { CSProjectVersion::VS2013, "12.0" },
            { CSProjectVersion::VS2015, "13.0" }, { CSProjectVersion::VS2017, "15.0" },
            { CSProjectVersion::VS2019, "16.0" }, { CSProjectVersion::MonoDevelop, "14.0" }
        };

        StringStream tempStream;
        for (const auto& scriptEntry : projectData.ScriptFiles)
            tempStream << fmt::format(ScriptEntryTemplate, scriptEntry.string());

        const String scriptEntries = tempStream.str();
        tempStream.str("");
        tempStream.clear();

        for (const auto& nonScriptEntry : projectData.NonScriptFiles)
            tempStream << fmt::format(NonScriptEntryTemplate, nonScriptEntry.string());

        const String nonScriptEntries = tempStream.str();
        tempStream.str();
        tempStream.clear();

        for (const ScriptProjectReference& assemblyRef : projectData.AssemblyReferences)
        {
            String refName = assemblyRef.Name;
            if (assemblyRef.Filepath.empty())
                tempStream << fmt::format(ReferenceEntryTemplate, refName);
            else
                tempStream << fmt::format(ReferencePathEntryTemplate, refName, assemblyRef.Filepath.string());
        }

        const String refEntries = tempStream.str();
        tempStream.str("");
        tempStream.clear();

        for (const ScriptProjectReference& projectRef : projectData.ProjectReferences)
        {
            String refName = projectRef.Name;
            String projectGUID = GetProjectGUID(projectRef.Name);
            tempStream << fmt::format(ReferenceProjectEntryTemplate, refName, projectGUID);
        }

        const String projectRefEntries = tempStream.str();
        tempStream.str();
        tempStream.clear();

        tempStream << projectData.Defines;

        const String defines = tempStream.str();
        const String projectGUID = GetProjectGUID(projectData.Name);

        String langVersion = "9.0";
        return fmt::format(ProjectTemplate, versionData[projectVersion], langVersion, projectGUID,
                                   projectData.Name, defines, refEntries, projectRefEntries, scriptEntries,
                                   nonScriptEntries);
    }
} // namespace Crowny