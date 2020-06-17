#include "cwpch.h"
#include "Crowny.h"
<<<<<<< HEAD
=======
#include <iostream>
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2

class Test : public Crowny::Application
{
public:
	Test(int argc, char** argv)
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
<<<<<<< HEAD
=======
	std::cout << "Is it working?" << std::endl;
>>>>>>> 8d51831a55da8001ceaabdbd722f54bfd1f9b2a2
	Crowny::Log::Init();

	auto app = new Test(argc, argv);
	app->Run();
	delete app;
}