#include "cwepch.h"

#include "Editor/Script/VisualStudioCodeEditor.h"
#include "Editor/Script/ScriptProjectGenerator.h"

#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/FileSystem.h"

namespace Crowny
{
	void VisualStudioCodeEditor::OpenFile(const Path& solitionPath, const Path& filePath, uint32_t line) const {

	}

	void VisualStudioCodeEditor::Sync(const CodeSolutionData& data, const Path& solutionPath) const
	{
        String solutionString = CSProject::GenerateSolution(CSProjectVersion::VS2017, data);
        solutionString = StringUtils::Replace(solutionString, "\n", "\n\r");
        Path solutionPathCopy = solutionPath;
        solutionPathCopy = solutionPath / (data.Name + ".sln");

		for (const CodeProjectData& project : data.Projects)
		{
            String projectString = CSProject::GenerateProject(CSProjectVersion::VS2017, project);
            projectString = StringUtils::Replace(projectString, "\n", "\n\r");

			Path projectPath = solutionPath / (project.Name + ".csproj");

			Ref<DataStream> projectStream = FileSystem::CreateAndOpenFile(projectPath);
            projectStream->Write(projectString.c_str(), projectString.size() * sizeof(String::value_type));
			projectStream->Close();
		}

		Ref<DataStream> solutionStream = FileSystem::CreateAndOpenFile(solutionPathCopy);
        solutionStream->Write(solutionString.c_str(), solutionString.size() * sizeof(String::value_type));
		solutionStream->Close();
	}
}