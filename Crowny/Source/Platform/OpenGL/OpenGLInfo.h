#pragma once

namespace Crowny
{
	struct OpenGLDetail
	{
		std::string Name;
		std::string GLName;
		std::string Value;
	};

	class OpenGLInfo
	{

	public: 
		static std::vector<OpenGLDetail>& GetInformation();
		static void RetrieveInformation();
	private:
		static std::vector<OpenGLDetail> s_Information;
	};
}