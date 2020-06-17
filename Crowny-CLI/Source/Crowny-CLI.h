#pragma once

#include <msclr\marshal_cppstd.h>

namespace Crowny
{

	static System::String^ std_string_to_string(const std::string& string)
	{
		return msclr::interop::marshal_as<System::String^>(string);
	}

	template<typename T>
	public ref class ManagedClass
	{
	protected:
		T* m_Instance;

	public:
		ManagedClass()
		{
			
		}

		ManagedClass(T* instance) : m_Instance(instance)
		{

		}

		virtual ~ManagedClass()
		{
			if (m_Instance != nullptr)
			{
				delete m_Instance;
				m_Instance = nullptr;
			}
		}

		!ManagedClass()
		{
			if (m_Instance != nullptr)
			{
				delete m_Instance;
				m_Instance = nullptr;
			}
		}
		
		T* GetHandle()
		{
			return m_Instance;
		}

	};

}