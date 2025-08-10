#pragma once
#include <iostream>

namespace Dingo
{

	using KeyCode = uint16_t;

	namespace Key
	{
		enum : KeyCode
		{
			// From glfw3.h
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
		};
	}

	inline std::ostream& operator<<(std::ostream& os, KeyCode keyCode)
	{
		os << static_cast<int32_t>(keyCode);
		return os;
	}

}


#if 1
// From glfw3.h
#define AT_KEY_SPACE           ::Dingo::Key::Space
#define AT_KEY_APOSTROPHE      ::Dingo::Key::Apostrophe    /* ' */
#define AT_KEY_COMMA           ::Dingo::Key::Comma         /* , */
#define AT_KEY_MINUS           ::Dingo::Key::Minus         /* - */
#define AT_KEY_PERIOD          ::Dingo::Key::Period        /* . */
#define AT_KEY_SLASH           ::Dingo::Key::Slash         /* / */
#define AT_KEY_0               ::Dingo::Key::D0
#define AT_KEY_1               ::Dingo::Key::D1
#define AT_KEY_2               ::Dingo::Key::D2
#define AT_KEY_3               ::Dingo::Key::D3
#define AT_KEY_4               ::Dingo::Key::D4
#define AT_KEY_5               ::Dingo::Key::D5
#define AT_KEY_6               ::Dingo::Key::D6
#define AT_KEY_7               ::Dingo::Key::D7
#define AT_KEY_8               ::Dingo::Key::D8
#define AT_KEY_9               ::Dingo::Key::D9
#define AT_KEY_SEMICOLON       ::Dingo::Key::Semicolon     /* ; */
#define AT_KEY_EQUAL           ::Dingo::Key::Equal         /* = */
#define AT_KEY_A               ::Dingo::Key::A
#define AT_KEY_B               ::Dingo::Key::B
#define AT_KEY_C               ::Dingo::Key::C
#define AT_KEY_D               ::Dingo::Key::D
#define AT_KEY_E               ::Dingo::Key::E
#define AT_KEY_F               ::Dingo::Key::F
#define AT_KEY_G               ::Dingo::Key::G
#define AT_KEY_H               ::Dingo::Key::H
#define AT_KEY_I               ::Dingo::Key::I
#define AT_KEY_J               ::Dingo::Key::J
#define AT_KEY_K               ::Dingo::Key::K
#define AT_KEY_L               ::Dingo::Key::L
#define AT_KEY_M               ::Dingo::Key::M
#define AT_KEY_N               ::Dingo::Key::N
#define AT_KEY_O               ::Dingo::Key::O
#define AT_KEY_P               ::Dingo::Key::P
#define AT_KEY_Q               ::Dingo::Key::Q
#define AT_KEY_R               ::Dingo::Key::R
#define AT_KEY_S               ::Dingo::Key::S
#define AT_KEY_T               ::Dingo::Key::T
#define AT_KEY_U               ::Dingo::Key::U
#define AT_KEY_V               ::Dingo::Key::V
#define AT_KEY_W               ::Dingo::Key::W
#define AT_KEY_X               ::Dingo::Key::X
#define AT_KEY_Y               ::Dingo::Key::Y
#define AT_KEY_Z               ::Dingo::Key::Z
#define AT_KEY_LEFT_BRACKET    ::Dingo::Key::LeftBracket   /* [ */
#define AT_KEY_BACKSLASH       ::Dingo::Key::Backslash     /* \ */
#define AT_KEY_RIGHT_BRACKET   ::Dingo::Key::RightBracket  /* ] */
#define AT_KEY_GRAVE_ACCENT    ::Dingo::Key::GraveAccent   /* ` */
#define AT_KEY_WORLD_1         ::Dingo::Key::World1        /* non-US #1 */
#define AT_KEY_WORLD_2         ::Dingo::Key::World2        /* non-US #2 */

/* Function keys */
#define AT_KEY_ESCAPE          ::Dingo::Key::Escape
#define AT_KEY_ENTER           ::Dingo::Key::Enter
#define AT_KEY_TAB             ::Dingo::Key::Tab
#define AT_KEY_BACKSPACE       ::Dingo::Key::Backspace
#define AT_KEY_INSERT          ::Dingo::Key::Insert
#define AT_KEY_DELETE          ::Dingo::Key::Delete
#define AT_KEY_RIGHT           ::Dingo::Key::Right
#define AT_KEY_LEFT            ::Dingo::Key::Left
#define AT_KEY_DOWN            ::Dingo::Key::Down
#define AT_KEY_UP              ::Dingo::Key::Up
#define AT_KEY_PAGE_UP         ::Dingo::Key::PageUp
#define AT_KEY_PAGE_DOWN       ::Dingo::Key::PageDown
#define AT_KEY_HOME            ::Dingo::Key::Home
#define AT_KEY_END             ::Dingo::Key::End
#define AT_KEY_CAPS_LOCK       ::Dingo::Key::CapsLock
#define AT_KEY_SCROLL_LOCK     ::Dingo::Key::ScrollLock
#define AT_KEY_NUM_LOCK        ::Dingo::Key::NumLock
#define AT_KEY_PRINT_SCREEN    ::Dingo::Key::PrintScreen
#define AT_KEY_PAUSE           ::Dingo::Key::Pause
#define AT_KEY_F1              ::Dingo::Key::F1
#define AT_KEY_F2              ::Dingo::Key::F2
#define AT_KEY_F3              ::Dingo::Key::F3
#define AT_KEY_F4              ::Dingo::Key::F4
#define AT_KEY_F5              ::Dingo::Key::F5
#define AT_KEY_F6              ::Dingo::Key::F6
#define AT_KEY_F7              ::Dingo::Key::F7
#define AT_KEY_F8              ::Dingo::Key::F8
#define AT_KEY_F9              ::Dingo::Key::F9
#define AT_KEY_F10             ::Dingo::Key::F10
#define AT_KEY_F11             ::Dingo::Key::F11
#define AT_KEY_F12             ::Dingo::Key::F12
#define AT_KEY_F13             ::Dingo::Key::F13
#define AT_KEY_F14             ::Dingo::Key::F14
#define AT_KEY_F15             ::Dingo::Key::F15
#define AT_KEY_F16             ::Dingo::Key::F16
#define AT_KEY_F17             ::Dingo::Key::F17
#define AT_KEY_F18             ::Dingo::Key::F18
#define AT_KEY_F19             ::Dingo::Key::F19
#define AT_KEY_F20             ::Dingo::Key::F20
#define AT_KEY_F21             ::Dingo::Key::F21
#define AT_KEY_F22             ::Dingo::Key::F22
#define AT_KEY_F23             ::Dingo::Key::F23
#define AT_KEY_F24             ::Dingo::Key::F24
#define AT_KEY_F25             ::Dingo::Key::F25

/* Keypad */
#define AT_KEY_KP_0            ::Dingo::Key::KP0
#define AT_KEY_KP_1            ::Dingo::Key::KP1
#define AT_KEY_KP_2            ::Dingo::Key::KP2
#define AT_KEY_KP_3            ::Dingo::Key::KP3
#define AT_KEY_KP_4            ::Dingo::Key::KP4
#define AT_KEY_KP_5            ::Dingo::Key::KP5
#define AT_KEY_KP_6            ::Dingo::Key::KP6
#define AT_KEY_KP_7            ::Dingo::Key::KP7
#define AT_KEY_KP_8            ::Dingo::Key::KP8
#define AT_KEY_KP_9            ::Dingo::Key::KP9
#define AT_KEY_KP_DECIMAL      ::Dingo::Key::KPDecimal
#define AT_KEY_KP_DIVIDE       ::Dingo::Key::KPDivide
#define AT_KEY_KP_MULTIPLY     ::Dingo::Key::KPMultiply
#define AT_KEY_KP_SUBTRACT     ::Dingo::Key::KPSubtract
#define AT_KEY_KP_ADD          ::Dingo::Key::KPAdd
#define AT_KEY_KP_ENTER        ::Dingo::Key::KPEnter
#define AT_KEY_KP_EQUAL        ::Dingo::Key::KPEqual

#define AT_KEY_LEFT_SHIFT      ::Dingo::Key::LeftShift
#define AT_KEY_LEFT_CONTROL    ::Dingo::Key::LeftControl
#define AT_KEY_LEFT_ALT        ::Dingo::Key::LeftAlt
#define AT_KEY_LEFT_SUPER      ::Dingo::Key::LeftSuper
#define AT_KEY_RIGHT_SHIFT     ::Dingo::Key::RightShift
#define AT_KEY_RIGHT_CONTROL   ::Dingo::Key::RightControl
#define AT_KEY_RIGHT_ALT       ::Dingo::Key::RightAlt
#define AT_KEY_RIGHT_SUPER     ::Dingo::Key::RightSuper
#define AT_KEY_MENU            ::Dingo::Key::Menu
#endif
