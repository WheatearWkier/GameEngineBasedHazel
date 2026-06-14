#pragma once

#include "Hazel/Core/Core.h"
#include <string>
#include <functional>

namespace Hazel {

	//class是强枚举类型，它不能被当为整数，除非强制转换，因此保证使用安全，必须得加上作用域::来使用
	enum class EventType
	{
		None = 0,
		WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
		AppTick, AppUpdate, AppRender,
		KeyPressed, KeyReleased, KeyTyped,
		MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
	};

	enum EventCategory
	{
		None = 0,
		EventCategoryApplication	= BIT(0),
		EventCategoryInput			= BIT(1),
		EventCategoryKeyboard		= BIT(2),
		EventCategoryMouse			= BIT(3),
		EventCategoryMouseButton	= BIT(4),
	};

	//双井号##表示前后拼接，因为强枚举一定要带上作用域，所以用双井号拼接；单井号是把变量变为字符串
	//不用双井号编译器直接把EventType::type当成EventType::和type了，return了EventType::
#define EVENT_CLASS_TYPE(type) static EventType GetStaticType() { return EventType::##type; }\
								virtual EventType GetEventType() const override { return GetStaticType(); }\
								virtual const char* GetName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category) virtual int GetCategoryFlags() const override { return category; }

	class HAZEL_API Event
	{
		friend class EventDispatcher;
	public:
		/*virtual xxx xxx() const是一个完整的接口，const保证函数里的对象不可被改变，所以
		virtual xxx xxx() const override其实是对virtual xxx xxx() const重载
		如果有void PrintEvent(const Event& e)传入const的e，那么里面调用的e.GetEventType()这个函数声明时后面应该有const*/
		virtual EventType GetEventType() const = 0;
		virtual const char* GetName() const = 0;
		virtual int GetCategoryFlags() const = 0;
		virtual std::string ToString() const { return GetName(); }

		inline bool IsInCategory(EventCategory category) {
			return GetCategoryFlags() & category;
		}

		inline bool Handled() const { return m_Handled; }

		bool m_Handled = false;
	};

	class EventDispatcher
	{
		/*std::function<>是std的一个函数封装器，可以封装包括lamba和bind的各种函数；
		bool(T&)是函数签名，可以理解为 函数指针 bool (*)(T&) 的简化形式*/
		template<typename T>
		using EventFn = std::function<bool(T&)>;
	public:
		EventDispatcher(Event& event) : m_Event(event)
		{
		}

		//函数参数是函数指针，带模板，故也需要template
		template<typename T>
		bool Dispatch(EventFn<T> func) {
			if (m_Event.GetEventType() == T::GetStaticType()) {
				//*(T*)&m_Event是强制类型转换，把&m_Event转换成T*指针，再*取出变成T&，这样就可以运行func函数
				m_Event.m_Handled = func(*(T*)&m_Event);
				return true;
			}
			return false;
		}
	private:
		Event& m_Event;
	};

	inline std::ostream& operator<<(std::ostream& os, const Event& e) {
		return os << e.ToString();
	}
}