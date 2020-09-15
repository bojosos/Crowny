#pragma once

#include "Crowny/Common/Common.h"
#include "Crowny/Input/KeyCodes.h"
#include "Crowny/Input/MouseCodes.h"

namespace Crowny
{

	enum class Cursor
	{
		NO_CURSOR,
		POINTER,
		IBEAM,
		CROSSHAIR,
		HAND,
		HRESIZE,
		VRESIZE
	};

	class Input
	{
	public:
		static bool IsKeyPressed(KeyCode key);
		static bool IsMouseButtonPressed(MouseCode button);
		static glm::vec2 GetMousePosition();
		static float GetMouseX();
		static float GetMouseY();
		static void SetMouseGrabbed(bool grabbed);
		static bool IsMouseGrabbed();
		static void SetMousePosition(const glm::vec2& position);
		//static uint32_t GetMouseScroll();
	private:
		static bool s_Grabbed;
	};
}