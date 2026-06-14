#include "hzpch.h"
#include "OpenGLContext.h"

namespace Hazel {

	Hazel::OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		HZ_CORE_ASSERT(windowHandle, "Window handle is null!");
	}

	void Hazel::OpenGLContext::Init()
	{
		HZ_PROFILE_FUNCTION();

		glfwMakeContextCurrent(m_WindowHandle);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		HZ_CORE_ASSERT(status, "Failed to initialize Glad!");

		HZ_CORE_INFO("OpenGL Info:");
		HZ_CORE_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
		HZ_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
		HZ_CORE_INFO("  Version: {0}", (const char*)glGetString(GL_VERSION));
	}

	void Hazel::OpenGLContext::SwapBuffers()
	{
		HZ_PROFILE_FUNCTION();

		glfwSwapBuffers(m_WindowHandle);
	}

}

