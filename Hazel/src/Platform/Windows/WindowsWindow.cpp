#include "hzpch.h"
#include "WindowsWindow.h"

#include "Hazel/Events/KeyEvent.h"
#include "Hazel/Events/MouseEvent.h"
#include "Hazel/Events/ApplicationEvent.h"

#include "Platform/OpenGL/OpenGLContext.h"

#include "stb_image.h"

namespace Hazel {

    // 记录当前已初始化的 GLFW 窗口数量
    // 第一个窗口创建时初始化 GLFW，最后一个销毁时终止 GLFW
    static uint32_t s_GLFWWindowCount = 0;

    static void GLFWErrorCallback(int error, const char* description)
    {
        HZ_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
    }

    // ═══════════════════════════════════════════════════════
    //  Window 基类工厂（平台层实现）
    // ═══════════════════════════════════════════════════════

    Window* Window::Create(const WindowProps& props)
    {
        return new WindowsWindow(props);
    }

    // ═══════════════════════════════════════════════════════
    //  构造 / 析构
    // ═══════════════════════════════════════════════════════

    WindowsWindow::WindowsWindow(const WindowProps& props)
    {
        HZ_PROFILE_FUNCTION();
        Init(props);
    }

    WindowsWindow::~WindowsWindow()
    {
        HZ_PROFILE_FUNCTION();
        Shutdown();
    }

    // ═══════════════════════════════════════════════════════
    //  初始化
    // ═══════════════════════════════════════════════════════

    void WindowsWindow::Init(const WindowProps& props)
    {
        HZ_PROFILE_FUNCTION();

        m_Data.Title = props.Title;
        m_Data.Width = props.Width;
        m_Data.Height = props.Height;

        HZ_CORE_INFO("Creating window '{0}' ({1} x {2})",
            props.Title, props.Width, props.Height);

        // 第一个窗口时初始化 GLFW
        if (s_GLFWWindowCount == 0)
        {
            HZ_PROFILE_SCOPE("glfwInit");
            const int success = glfwInit();
            HZ_CORE_ASSERT(success, "Failed to initialize GLFW");
            glfwSetErrorCallback(GLFWErrorCallback);
        }

        {
            HZ_PROFILE_SCOPE("glfwCreateWindow");
            m_Window = glfwCreateWindow(
                static_cast<int>(props.Width),
                static_cast<int>(props.Height),
                props.Title.c_str(),
                nullptr, nullptr
            );
            s_GLFWWindowCount++;

            // ── 设置窗口图标 ──────────────────────────────────────
            if (!props.IconPath.empty())
            {
                GLFWimage icon;
                icon.pixels = stbi_load(props.IconPath.c_str(),
                    &icon.width, &icon.height, nullptr, 4);
                if (icon.pixels)
                {
                    glfwSetWindowIcon(m_Window, 1, &icon);
                    stbi_image_free(icon.pixels);
                }
            }
        }

        // 初始化图形上下文（OpenGL: 创建并设置当前 context）
        m_Context = CreateScope<OpenGLContext>(m_Window);
        m_Context->Init();

        // 把 WindowData 的地址存入 GLFW，供回调函数取回
        // 原因：GLFW 回调是 C 函数指针，不能捕获 this，
        // 只能通过 UserPointer 把需要的数据传递进去
        glfwSetWindowUserPointer(m_Window, &m_Data);
        SetVSync(true);

        SetupCallbacks();
    }

    // ═══════════════════════════════════════════════════════
    //  回调注册（拆分出来让 Init 不过长）
    // ═══════════════════════════════════════════════════════

    void WindowsWindow::SetupCallbacks()
    {
        // 取 UserPointer 的辅助 lambda，所有回调都用这个模式：
        // 1. 从 GLFW 取回 WindowData
        // 2. 构造引擎事件对象
        // 3. 调用 EventCallback，把事件派发给 Application

        // ── 窗口尺寸改变 ─────────────────────────────────
        glfwSetWindowSizeCallback(m_Window,
            [](GLFWwindow* window, int width, int height)
            {
                auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
                data.Width = static_cast<uint32_t>(width);
                data.Height = static_cast<uint32_t>(height);

                WindowResizeEvent event(width, height);
                data.EventCallback(event);
            });

        // ── 窗口关闭 ──────────────────────────────────────
        glfwSetWindowCloseCallback(m_Window,
            [](GLFWwindow* window)
            {
                auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
                WindowCloseEvent event;
                data.EventCallback(event);
            });

        // ── 键盘按键 ──────────────────────────────────────
        glfwSetKeyCallback(m_Window,
            [](GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/)
            {
                auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
                switch (action)
                {
                case GLFW_PRESS:
                {
                    KeyPressedEvent event(key, 0);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    KeyReleasedEvent event(key);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_REPEAT:
                {
                    KeyPressedEvent event(key, 1);
                    data.EventCallback(event);
                    break;
                }
                }
            });

        // ── 字符输入（用于文本框等 UI 输入）──────────────
        glfwSetCharCallback(m_Window,
            [](GLFWwindow* window, unsigned int codepoint)
            {
                auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
                KeyTypedEvent event(codepoint);
                data.EventCallback(event);
            });

        // ── 鼠标按键 ──────────────────────────────────────
        glfwSetMouseButtonCallback(m_Window,
            [](GLFWwindow* window, int button, int action, int /*mods*/)
            {
                auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
                switch (action)
                {
                case GLFW_PRESS:
                {
                    MouseButtonPressedEvent event(button);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    MouseButtonReleasedEvent event(button);
                    data.EventCallback(event);
                    break;
                }
                }
            });

        // ── 鼠标滚轮 ──────────────────────────────────────
        glfwSetScrollCallback(m_Window,
            [](GLFWwindow* window, double xOffset, double yOffset)
            {
                auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
                MouseScrolledEvent event(
                    static_cast<float>(xOffset),
                    static_cast<float>(yOffset)
                );
                data.EventCallback(event);
            });

        // ── 鼠标移动 ──────────────────────────────────────
        glfwSetCursorPosCallback(m_Window,
            [](GLFWwindow* window, double xPos, double yPos)
            {
                auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
                MouseMovedEvent event(
                    static_cast<float>(xPos),
                    static_cast<float>(yPos)
                );
                data.EventCallback(event);
            });
    }

    // ═══════════════════════════════════════════════════════
    //  关闭
    // ═══════════════════════════════════════════════════════

    void WindowsWindow::Shutdown()
    {
        HZ_PROFILE_FUNCTION();

        glfwDestroyWindow(m_Window);
        s_GLFWWindowCount--;

        // 最后一个窗口销毁时终止 GLFW
        if (s_GLFWWindowCount == 0)
            glfwTerminate();
    }

    // ═══════════════════════════════════════════════════════
    //  每帧更新
    // ═══════════════════════════════════════════════════════

    void WindowsWindow::OnUpdate()
    {
        HZ_PROFILE_FUNCTION();

        m_Context->SwapBuffers(); // 交换前后缓冲，显示当前帧
    }

    // ═══════════════════════════════════════════════════════
    //  VSync
    // ═══════════════════════════════════════════════════════

    void WindowsWindow::SetVSync(bool enabled)
    {
        HZ_PROFILE_FUNCTION();

        // 0 = 不等待垂直同步（不限帧率），1 = 等待（锁 60fps）
        glfwSwapInterval(enabled ? 1 : 0);
        m_Data.VSync = enabled;
    }

    bool WindowsWindow::IsVSync() const
    {
        return m_Data.VSync;
    }

} // namespace Hazel