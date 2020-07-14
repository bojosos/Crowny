#pragma once

#include "Crowny/Common/Common.h"
#include "Crowny/Common/Log.h"

extern Crowny::Application* Crowny::CreateApplication();

int main(int argc, char** argv)
{
	Crowny::Log::Init();

	auto app = Crowny::CreateApplication();
	app->Run();
	delete app;
}
