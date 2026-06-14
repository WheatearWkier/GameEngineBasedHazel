#pragma once

#include "Hazel/Core/Core.h"
#include "Hazel/Core/KeyCodes.h"
//#include "Hazel/Core/MouseCodes.h"

namespace Hazel {

	enum class CursorMode
	{
		Normal = 0,
		Hidden,
		Locked
	};

	class Input
	{
	public:
		static bool IsKeyPressed(int keycode);
		static bool IsMouseButtonPressed(int button);
		static std::pair<float, float> GetMousePosition();
		static float GetMouseX();
		static float GetMouseY();

		static void SetCursorMode(CursorMode mode);
	};

}