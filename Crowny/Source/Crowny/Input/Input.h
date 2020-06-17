#pragma once

#include "cwpch.h"
#include "keycodes.h"
#include "mousecodes.h"

#include <glm/glm.hpp>

namespace Crowny
{
	enum class CursorType
	{
		NO_CURSOR,
		POINTER
	};
	class Input
	{
	public:
		inline static bool IsKeyPressed(KeyCode key) { return s_Instance->IsKeyPressedImpl(key); }

		inline static bool IsMouseButtonPressed(MouseCode button) { return s_Instance->IsMouseButtonPressedImpl(button); }

		inline static glm::vec2 GetMousePosition() { return s_Instance->GetMousePositionImpl(); };

		inline static float GetMouseX() { return s_Instance->GetMouseXImpl(); }
		inline static float GetMouseY() { return s_Instance->GetMouseYImpl(); }
		
		inline static bool IsMouseGrabbed() { return s_Instance->IsMouseGrabbedImpl(); }
		inline static void SetMousePosition(const glm::vec2& position) { s_Instance->SetMousePositionImpl(position); }
		inline static void SetMouseGrabbed(bool grabbed) { s_Instance->SetMouseGrabbedImpl(grabbed); }
		inline static void SetMouseCursor(CursorType cursor) { s_Instance->SetMouseCursorImpl(cursor); }

	private:
		inline bool IsMouseGrabbedImpl() { return m_Grabbed; }
		inline void SetMouseGrabbedImpl(bool grabbed) { m_Grabbed = grabbed; }
		void SetMousePositionImpl(const glm::vec2& position);
		void SetMouseCursorImpl(CursorType cursor);

		bool IsKeyPressedImpl(KeyCode key);
		bool IsMouseButtonPressedImpl(MouseCode button);
		glm::vec2 GetMousePositionImpl();
		float GetMouseXImpl();
		float GetMouseYImpl();
	private:
		bool m_Grabbed = false;
	private:
		static Scope<Input> s_Instance;
	};
}