#ifndef TEST__THREAD_H
#define TEST__THREAD_H

#include "type.h"

//! An example thread implementation.
struct t_thread : t_object<t_type>
{
	//! Required by recyclone.
	recyclone::t_thread<t_type>* v_internal = nullptr;

	//! To avoid zero initialization.
	t_thread()
	{
	}
	//! Called by t_type_of<t_thread>::f_scan(...).
	void f_scan(t_scan<t_type>)
	{
	}
};

struct t_engine_with_threads : t_engine<t_type>
{
	using t_engine<t_type>::t_engine;
	template<typename T>
	::t_thread* f_start_thread(T&& a_main, bool a_background = false)
	{
		auto RECYCLONE__SPILL thread = f_new<::t_thread>();
		f_start(thread, [=]
		{
			thread->v_internal->v_background = a_background;
		}, std::forward<T>(a_main));
		return thread;
	}
};

struct t_engine_with_finalizer : t_engine_with_threads
{
	t_engine_with_finalizer(t_options& a_options, void(*a_finalize)(t_object<t_type>*)) : t_engine_with_threads(a_options)
	{
		// Finalizer is an instance of recyclone::t_thread.
		v_thread__finalizer = f_start_thread([=]
		{
			f_finalizer(a_finalize);
		})->v_internal;
	}
};

template<typename T_do>
void f_padding(T_do a_do)
{
	char padding[4096];
	std::memset(padding, 0, sizeof(padding));
	a_do();
}

#endif
