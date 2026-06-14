#pragma once

#include "Hazel/Core/Window.h"
#include "Hazel/Renderer/GraphicsContext.h"

struct GLFWwindow;

namespace Hazel {

    class WindowsWindow : public Window
    {
    public:
        WindowsWindow(const WindowProps& props);
        virtual ~WindowsWindow();

        void OnUpdate() override;

        uint32_t GetWidth()  const override { return m_Data.Width; }
        uint32_t GetHeight() const override { return m_Data.Height; }

        void SetEventCallback(const EventCallbackFn& callback) override
        {
            m_Data.EventCallback = callback;
        }

        void SetVSync(bool enabled) override;
        bool IsVSync() const override;

        void* GetNativeWindow() const override { return m_Window; }

    private:
        void Init(const WindowProps& props);
        void Shutdown();

        // GLFW 回调注册（拆分出来让 Init 更清晰）
        void SetupCallbacks();

    private:
        GLFWwindow* m_Window = nullptr;
        Scope<GraphicsContext>   m_Context;

        // WindowData 只存需要在 GLFW 回调里访问的数据
        // GLFW 回调是 C 函数指针，无法捕获 this，
        // 所以把必要数据打包成这个结构体，通过 glfwSetWindowUserPointer 传递
        struct WindowData
        {
            std::string     Title;
            uint32_t        Width = 0;
            uint32_t        Height = 0;
            bool            VSync = false;
            EventCallbackFn EventCallback;  // 绑定到 Application::OnEvent
        };

        WindowData m_Data;
    };

} // namespace Hazel