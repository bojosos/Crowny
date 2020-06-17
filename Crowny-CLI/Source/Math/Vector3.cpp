#include "Vector3.h"
#include <glm/gtx/string_cast.hpp>

namespace Crowny
{

	Vector3::Vector3(glm::vec3* instance) : ManagedClass(instance)
	{

	}

	Vector3::Vector3() : ManagedClass()
	{

	}

	Vector3::Vector3(float x, float y, float z)
	{
		m_Instance = new glm::vec3(x, y, z);
	}

	//Vector3::Vector3(Vector2^ vector)
	//{

	//}

	Crowny::Vector3^ Vector3::operator+(Vector3^ left, Vector3^ right)
	{
		return gcnew Vector3(&(*left->GetHandle() + *right->GetHandle()));
	}

	Crowny::Vector3^ Vector3::operator-(Vector3^ left, Vector3^ right)
	{
		return gcnew Vector3(&(*left->GetHandle() - *right->GetHandle()));
	}

	Crowny::Vector3^ Vector3::operator*(Vector3^ left, Vector3^ right)
	{
		return gcnew Vector3(&(*left->GetHandle() * *right->GetHandle()));
	}

	Crowny::Vector3^ Vector3::operator/(Vector3^ left, Vector3^ right)
	{
		return gcnew Vector3(&(*left->GetHandle() / *right->GetHandle()));
	}

	bool Vector3::operator==(Vector3^ other)
	{
		return *m_Instance == *other->GetHandle();
	}

	bool Vector3::operator!=(Vector3^ other)
	{
		return *m_Instance != *other->GetHandle();
	}

	Vector3^ Vector3::operator+=(Vector3^ other)
	{
		return this + other;
	}

	Vector3^ Vector3::operator-=(Vector3^ other)
	{
		return this - other;
	}

	Vector3^ Vector3::operator*=(Vector3^ other)
	{
		return this * other;
	}

	Vector3^ Vector3::operator/=(Vector3^ other)
	{
		return this / other;
	}

	float Vector3::Distance(Vector3^ left, Vector3^ right)
	{
		return glm::distance(*left->GetHandle(), *right->GetHandle());
	}

	System::String^ Vector3::ToString()
	{
		return std_string_to_string(glm::to_string(*m_Instance));
	}
}