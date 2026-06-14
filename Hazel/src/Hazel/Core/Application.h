#pragma once
#include "hzpch.h"
#include "Core.h"

#include "Window.h"
#include "Hazel/Core/LayerStack.h"
#include "Hazel/Events/Event.h"
#include "Hazel/Events/ApplicationEvent.h"

#include "Hazel/Core/Timestep.h"

#include "Hazel/ImGui/ImGuiLayer.h"

namespace Hazel {

	struct ApplicationCommandLineArgs
	{
		int Count = 0;
		char** Args = nullptr;

		const char* operator[](int index) const
		{
			HZ_CORE_ASSERT(index < Count);
			return Args[index];
		}
	};

	class Application 
	{
	public:
		Application(const std::string& name = "Hazel App", ApplicationCommandLineArgs args = ApplicationCommandLineArgs());
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PopLayer(Layer* layer);
		void PushOverlay(Layer* overlay);
		void PopOverlay(Layer* overlay);

		inline Window& GetWindow() { return *m_Window; }

		void Close();

		ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }

		inline static Application& Get() { return *s_Instance; }

		ApplicationCommandLineArgs GetCommandLineArgs() const { return m_CommandLineArgs; }
	
	private:
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);
		void PostUpdateLayers();

	private:
		ApplicationCommandLineArgs m_CommandLineArgs;
		std::unique_ptr<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		bool m_Running = true;
		bool m_Minized = false;
		
		LayerStack m_LayerStack;
		// ÑÓ³Ù²Ù×÷¶ÓÁÐ
		std::vector<Layer*> m_PendingLayersToPush;
		std::vector<Layer*> m_PendingLayersToPop;
		std::vector<Layer*> m_PendingOverlaysToPush;
		std::vector<Layer*> m_PendingOverlaysToPop;
		
		float m_LastFrameTime = 0.0f;

		static Application* s_Instance;
		//friend int ::main(int argc, char** argv);
	};

	//To be define in Client
	Application* CreateApplication(ApplicationCommandLineArgs args);

}


