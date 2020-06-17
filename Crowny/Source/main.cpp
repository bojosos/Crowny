#include "cwpch.h"
#include "Crowny.h"

class Test : public Crowny::Application
{
public:
	Test() : Application("test")
	{

	}

	void OnUpdate(Crowny::Timestep ts)
	{

	}

	~Test()
	{

	}
};

int main(int argc, char** argv)
{
	Crowny::Log::Init();

	auto* app = new Test();
	app->Run();
	delete app;
}