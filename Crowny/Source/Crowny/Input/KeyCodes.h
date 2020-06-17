#pragma once

namespace Crowny
{
	typedef enum class KeyCode : uint16_t
	{
		Space = 32,
		Apostrophe = 39, /* ' */
		Comma = 44, /* , */
		Minus = 45, /* - */
		Period = 46, /* . */
		Slash = 47, /* / */

		D0 = 48, /* 0 */
		D1 = 49, /* 1 */
		D2 = 50, /* 2 */
		D3 = 51, /* 3 */
		D4 = 52, /* 4 */
		D5 = 53, /* 5 */
		D6 = 54, /* 6 */
		D7 = 55, /* 7 */
		D8 = 56, /* 8 */
		D9 = 57, /* 9 */

		Semicolon = 59, /* ; */
		Equal = 61, /* = */

		A = 65,
		B = 66,
		C = 67,
		D = 68,
		E = 69,
		F = 70,
		G = 71,
		H = 72,
		I = 73,
		J = 74,
		K = 75,
		L = 76,
		M = 77,
		N = 78,
		O = 79,
		P = 80,
		Q = 81,
		R = 82,
		S = 83,
		T = 84,
		U = 85,
		V = 86,
		W = 87,
		X = 88,
		Y = 89,
		Z = 90,

		LeftBracket = 91,  /* [ */
		Backslash = 92,  /* \ */
		RightBracket = 93,  /* ] */
		GraveAccent = 96,  /* ` */

		World1 = 161, /* non-US #1 */
		World2 = 162, /* non-US #2 */

		/* Function keys */
		Escape = 256,
		Enter = 257,
		Tab = 258,
		Backspace = 259,
		Insert = 260,
		Delete = 261,
		Right = 262,
		Left = 263,
		Down = 264,
		Up = 265,
		PageUp = 266,
		PageDown = 267,
		Home = 268,
		End = 269,
		CapsLock = 280,
		ScrollLock = 281,
		NumLock = 282,
		PrintScreen = 283,
		Pause = 284,
		F1 = 290,
		F2 = 291,
		F3 = 292,
		F4 = 293,
		F5 = 294,
		F6 = 295,
		F7 = 296,
		F8 = 297,
		F9 = 298,
		F10 = 299,
		F11 = 300,
		F12 = 301,
		F13 = 302,
		F14 = 303,
		F15 = 304,
		F16 = 305,
		F17 = 306,
		F18 = 307,
		F19 = 308,
		F20 = 309,
		F21 = 310,
		F22 = 311,
		F23 = 312,
		F24 = 313,
		F25 = 314,

		/* Keypad */
		KP0 = 320,
		KP1 = 321,
		KP2 = 322,
		KP3 = 323,
		KP4 = 324,
		KP5 = 325,
		KP6 = 326,
		KP7 = 327,
		KP8 = 328,
		KP9 = 329,
		KPDecimal = 330,
		KPDivide = 331,
		KPMultiply = 332,
		KPSubtract = 333,
		KPAdd = 334,
		KPEnter = 335,
		KPEqual = 336,

		LeftShift = 340,
		LeftControl = 341,
		LeftAlt = 342,
		LeftSuper = 343,
		RightShift = 344,
		RightControl = 345,
		RightAlt = 346,
		RightSuper = 347,
		Menu = 348
	} Key;

	inline std::ostream& operator<<(std::ostream& os, KeyCode keyCode)
	{
		os << static_cast<int32_t>(keyCode);
		return os;
	}
}

#define KEY_SPACE           ::Minecraft::Key::Space
#define KEY_APOSTROPHE      ::Minecraft::Key::Apostrophe
#define KEY_COMMA           ::Minecraft::Key::Comma     
#define KEY_MINUS           ::Minecraft::Key::Minus     
#define KEY_PERIOD          ::Minecraft::Key::Period    
#define KEY_SLASH           ::Minecraft::Key::Slash     
#define KEY_0               ::Minecraft::Key::D0
#define KEY_1               ::Minecraft::Key::D1
#define KEY_2               ::Minecraft::Key::D2
#define KEY_3               ::Minecraft::Key::D3
#define KEY_4               ::Minecraft::Key::D4
#define KEY_5               ::Minecraft::Key::D5
#define KEY_6               ::Minecraft::Key::D6
#define KEY_7               ::Minecraft::Key::D7
#define KEY_8               ::Minecraft::Key::D8
#define KEY_9               ::Minecraft::Key::D9
#define KEY_SEMICOLON       ::Minecraft::Key::Semicolon 
#define KEY_EQUAL           ::Minecraft::Key::Equal
#define KEY_A               ::Minecraft::Key::A
#define KEY_B               ::Minecraft::Key::B
#define KEY_C               ::Minecraft::Key::C
#define KEY_D               ::Minecraft::Key::D
#define KEY_E               ::Minecraft::Key::E
#define KEY_F               ::Minecraft::Key::F
#define KEY_G               ::Minecraft::Key::G
#define KEY_H               ::Minecraft::Key::H
#define KEY_I               ::Minecraft::Key::I
#define KEY_J               ::Minecraft::Key::J
#define KEY_K               ::Minecraft::Key::K
#define KEY_L               ::Minecraft::Key::L
#define KEY_M               ::Minecraft::Key::M
#define KEY_N               ::Minecraft::Key::N
#define KEY_O               ::Minecraft::Key::O
#define KEY_P               ::Minecraft::Key::P
#define KEY_Q               ::Minecraft::Key::Q
#define KEY_R               ::Minecraft::Key::R
#define KEY_S               ::Minecraft::Key::S
#define KEY_T               ::Minecraft::Key::T
#define KEY_U               ::Minecraft::Key::U
#define KEY_V               ::Minecraft::Key::V
#define KEY_W               ::Minecraft::Key::W
#define KEY_X               ::Minecraft::Key::X
#define KEY_Y               ::Minecraft::Key::Y
#define KEY_Z               ::Minecraft::Key::Z
#define KEY_LEFT_BRACKET    ::Minecraft::Key::LeftBracket 
#define KEY_BACKSLASH       ::Minecraft::Key::Backslash   
#define KEY_RIGHT_BRACKET   ::Minecraft::Key::RightBracket
#define KEY_GRAVE_ACCENT    ::Minecraft::Key::GraveAccent 
#define KEY_WORLD_1         ::Minecraft::Key::World1      
#define KEY_WORLD_2         ::Minecraft::Key::World2      

#define KEY_ESCAPE          ::Minecraft::Key::Escape
#define KEY_ENTER           ::Minecraft::Key::Enter
#define KEY_TAB             ::Minecraft::Key::Tab
#define KEY_BACKSPACE       ::Minecraft::Key::Backspace
#define KEY_INSERT          ::Minecraft::Key::Insert
#define KEY_DELETE          ::Minecraft::Key::Delete
#define KEY_RIGHT           ::Minecraft::Key::Right
#define KEY_LEFT            ::Minecraft::Key::Left
#define KEY_DOWN            ::Minecraft::Key::Down
#define KEY_UP              ::Minecraft::Key::Up
#define KEY_PAGE_UP         ::Minecraft::Key::PageUp
#define KEY_PAGE_DOWN       ::Minecraft::Key::PageDown
#define KEY_HOME            ::Minecraft::Key::Home
#define KEY_END             ::Minecraft::Key::End
#define KEY_CAPS_LOCK       ::Minecraft::Key::CapsLock
#define KEY_SCROLL_LOCK     ::Minecraft::Key::ScrollLock
#define KEY_NUM_LOCK        ::Minecraft::Key::NumLock
#define KEY_PRINT_SCREEN    ::Minecraft::Key::PrintScreen
#define KEY_PAUSE           ::Minecraft::Key::Pause
#define KEY_F1              ::Minecraft::Key::F1
#define KEY_F2              ::Minecraft::Key::F2
#define KEY_F3              ::Minecraft::Key::F3
#define KEY_F4              ::Minecraft::Key::F4
#define KEY_F5              ::Minecraft::Key::F5
#define KEY_F6              ::Minecraft::Key::F6
#define KEY_F7              ::Minecraft::Key::F7
#define KEY_F8              ::Minecraft::Key::F8
#define KEY_F9              ::Minecraft::Key::F9
#define KEY_F10             ::Minecraft::Key::F10
#define KEY_F11             ::Minecraft::Key::F11
#define KEY_F12             ::Minecraft::Key::F12
#define KEY_F13             ::Minecraft::Key::F13
#define KEY_F14             ::Minecraft::Key::F14
#define KEY_F15             ::Minecraft::Key::F15
#define KEY_F16             ::Minecraft::Key::F16
#define KEY_F17             ::Minecraft::Key::F17
#define KEY_F18             ::Minecraft::Key::F18
#define KEY_F19             ::Minecraft::Key::F19
#define KEY_F20             ::Minecraft::Key::F20
#define KEY_F21             ::Minecraft::Key::F21
#define KEY_F22             ::Minecraft::Key::F22
#define KEY_F23             ::Minecraft::Key::F23
#define KEY_F24             ::Minecraft::Key::F24
#define KEY_F25             ::Minecraft::Key::F25

#define KEY_KP_0            ::Minecraft::Key::KP0
#define KEY_KP_1            ::Minecraft::Key::KP1
#define KEY_KP_2            ::Minecraft::Key::KP2
#define KEY_KP_3            ::Minecraft::Key::KP3
#define KEY_KP_4            ::Minecraft::Key::KP4
#define KEY_KP_5            ::Minecraft::Key::KP5
#define KEY_KP_6            ::Minecraft::Key::KP6
#define KEY_KP_7            ::Minecraft::Key::KP7
#define KEY_KP_8            ::Minecraft::Key::KP8
#define KEY_KP_9            ::Minecraft::Key::KP9
#define KEY_KP_DECIMAL      ::Minecraft::Key::KPDecimal
#define KEY_KP_DIVIDE       ::Minecraft::Key::KPDivide
#define KEY_KP_MULTIPLY     ::Minecraft::Key::KPMultiply
#define KEY_KP_SUBTRACT     ::Minecraft::Key::KPSubtract
#define KEY_KP_ADD          ::Minecraft::Key::KPAdd
#define KEY_KP_ENTER        ::Minecraft::Key::KPEnter
#define KEY_KP_EQUAL        ::Minecraft::Key::KPEqual

#define KEY_LEFT_SHIFT      ::Minecraft::Key::LeftShift
#define KEY_LEFT_CONTROL    ::Minecraft::Key::LeftControl
#define KEY_LEFT_ALT        ::Minecraft::Key::LeftAlt
#define KEY_LEFT_SUPER      ::Minecraft::Key::LeftSuper
#define KEY_RIGHT_SHIFT     ::Minecraft::Key::RightShift
#define KEY_RIGHT_CONTROL   ::Minecraft::Key::RightControl
#define KEY_RIGHT_ALT       ::Minecraft::Key::RightAlt
#define KEY_RIGHT_SUPER     ::Minecraft::Key::RightSuper
#define KEY_MENU            ::Minecraft::Key::Menu