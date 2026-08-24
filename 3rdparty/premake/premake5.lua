project "Premake"
	kind "Utility"
	local sanitizerArgument = _OPTIONS["sanitizer"] and (" --sanitizer=" .. _OPTIONS["sanitizer"]) or ""
	local simdArgument = _OPTIONS["simd"] and (" --simd=" .. _OPTIONS["simd"]) or ""

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{wks.location}/**premake5.lua"
	}

	postbuildmessage "Regenerating project files with Premake5!"
	postbuildcommands
	{
		"\"%{prj.location}bin/premake5\" %{_ACTION} --file=\"%{wks.location}premake5.lua\" --with-nodes" .. sanitizerArgument .. simdArgument
	}
