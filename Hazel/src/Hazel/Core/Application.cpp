#include "hzpch.h"
#include "Application.h"

#include "Hazel/Events/ApplicationEvent.h"
#include "Log.h"
#include "Input.h"
#include "Hazel/Audio/AudioEngine.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Hazel/Renderer/Renderer.h"
#include "Hazel/Scripting/ScriptEngine.h"

#include <thread>

namespace Hazel {

//OnEvent是一个成员函数app.OnEvent,要把他bind成一个普通函数Event供使用
//它生成了一个可调用对象,等价于 匿名函数 [this](Event& e) { this->OnEvent(e); }
//把“需要 this 的成员函数”，包装成“不需要 this 的普通函数”
#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

	Application* Application::s_Instance = nullptr;

	Application::Application(const std::string& name, ApplicationCommandLineArgs args) 
		: m_CommandLineArgs(args)
	{
		HZ_PROFILE_FUNCTION();

		HZ_CORE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;
		//应用程序直接使用之前创造好的窗口,这个窗口里已经准备好各种回调事件了
		m_Window = std::unique_ptr<Window>(Window::Create(WindowProps(name)));
		m_Window->SetEventCallback(BIND_EVENT_FN(OnEvent));

		Renderer::Init();
		ScriptEngine::Init();
		AudioEngine::Init();

		m_ImGuiLayer = new ImGuiLayer();
		PushOverlay(m_ImGuiLayer);
	}

	Application::~Application()
	{
		HZ_PROFILE_FUNCTION();
		ScriptEngine::Shutdown();
		AudioEngine::Shutdown();
	}

	void Application::PushLayer(Layer* layer)
	{
		HZ_PROFILE_FUNCTION();
		// 延迟处理，不直接 Push
		m_PendingLayersToPush.push_back(layer);
	}

	void Application::PopLayer(Layer* layer)
	{
		// 延迟处理，不直接 Pop
		m_PendingLayersToPop.push_back(layer);
	}

	void Application::PushOverlay(Layer* overlay)
	{
		HZ_PROFILE_FUNCTION();
		// 延迟处理，放入待推入 Overlay 队列
		m_PendingOverlaysToPush.push_back(overlay);
	}

	void Application::PopOverlay(Layer* overlay)
	{
		// 延迟处理，放入待弹出 Overlay 队列
		m_PendingOverlaysToPop.push_back(overlay);
	}

	void Application::Close()
	{
		m_Running = false;
	}

	void Application::OnEvent(Event& e)
	{
		HZ_PROFILE_FUNCTION();

		//各种事件都调用了OnEvent,宏观上就是所有事件传入这一个函数,因此要把事件分发
		//生成一个事件分发器(其实应该叫记录),它记录当前的事件e,然后判断要执行什么具体事件
		EventDispatcher dispatcher(e);
		//如果事件e是WindowCloseEvent类型,那么它将执行参数是WindowCloseEvent的函数OnWindowClose
		//并把e.m_Handled记录为true表示事件已执行
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(OnWindowResize));
		//HZ_CORE_TRACE("{0}", e.ToString());

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); ) {
			(*--it)->OnEvent(e);
			if (e.Handled())
				break;
		}
	}

	void Application::Run()
	{
		HZ_PROFILE_FUNCTION();

		// 在进入主循环前，先把构造函数里 Push 的 Layer / Overlay 冲刷进去，
		// 这样 m_ImGuiLayer 就能在第一帧开始前成功执行 OnAttach()
		PostUpdateLayers();

		while (m_Running)
		{
			HZ_PROFILE_SCOPE("RunLoop");

			// 处理所有待处理的 GLFW 事件
			glfwPollEvents();

			float nowTime = (float)glfwGetTime();
			Timestep timestep = nowTime - m_LastFrameTime;
			m_LastFrameTime = nowTime;

			if (!m_Minized)
			{
				{
					HZ_PROFILE_SCOPE("LayerStack OnUpdate");

					for (Layer* layer : m_LayerStack)
						layer->OnUpdate(timestep);
				}
				m_ImGuiLayer->Begin();
				{
					HZ_PROFILE_SCOPE("LayerStack OnImGuiRender");

					for (Layer* layer : m_LayerStack)
						layer->OnImGuiRender();
				}
				m_ImGuiLayer->End();
			}

			m_Window->OnUpdate();

			// 当所有 Layer 的遍历都结束了，安全地更新 Layer 状态
			PostUpdateLayers();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e) {
		m_Running = false;
		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		HZ_PROFILE_FUNCTION();

		if (e.GetWidth() == 0 || e.GetHeight() == 0) {
			m_Minized = true;
			return false;
		}
		m_Minized = false;
		Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());
		return true;
	}

	// 新增此函数，在安全的时候真正执行增删
	void Application::PostUpdateLayers()
	{
		// 1. 处理需要删除的 Layer
		for (Layer* layer : m_PendingLayersToPop)
		{
			m_LayerStack.PopLayer(layer);
			// delete layer; // 如果你的生命周期管理需要在此销毁，可以在这里 delete
		}
		m_PendingLayersToPop.clear();

		// 2. 处理需要删除的 Overlay（新增加）
		for (Layer* overlay : m_PendingOverlaysToPop)
		{
			m_LayerStack.PopOverlay(overlay);
		}
		m_PendingOverlaysToPop.clear();

		// 3. 处理需要推入的 Layer
		for (Layer* layer : m_PendingLayersToPush)
		{
			m_LayerStack.PushLayer(layer);
			layer->OnAttach();
		}
		m_PendingLayersToPush.clear();

		// 4. 处理需要推入的 Overlay（新增加）
		for (Layer* overlay : m_PendingOverlaysToPush)
		{
			m_LayerStack.PushOverlay(overlay);
			overlay->OnAttach(); // 确保 Overlay 也会触发 OnAttach
		}
		m_PendingOverlaysToPush.clear();
	}
}