#pragma once

#include "hzpch.h"

#include "Hazel/Core/Core.h"
#include "Hazel/Events/Event.h"

namespace Hazel {

	struct WindowProps
	{
		std::string Title;
		uint32_t Width;
		uint32_t Height;
		std::string IconPath;

		WindowProps(const std::string& title = "感觉不如原神",
			uint32_t width = 1920,
			uint32_t height = 1080,
			std::string iconPath = "Resources/Icons/icon.png")
			: Title(title), Width(width), Height(height), IconPath(iconPath)
		{
		}
	};

	// Interface representing a desktop system based Window
	class HAZEL_API Window
	{
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		virtual ~Window() {};

		virtual void OnUpdate() = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		// Window attributes
		virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
		virtual void SetVSync(bool enabled) = 0;
		virtual bool IsVSync() const = 0;

		virtual void* GetNativeWindow() const = 0;

		static Window* Create(const WindowProps& props = WindowProps());
	};

}