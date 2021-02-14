#ifndef RECYCLONE__THREAD_H
#define RECYCLONE__THREAD_H

#include "object.h"
#include <thread>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <sys/resource.h>

namespace recyclone
{

//! A set of thread data for garbage collection.
template<typename T_type>
class t_thread
{
	friend class t_weak_pointer<T_type>;
	friend class t_engine<T_type>;

	static inline thread_local t_thread* v_current;

	t_thread* v_next;
	/*!
	  Running status:
	    - -1: not started
	    - 0: running
	    - 1: done (increment)
	    - 2: done (decrement and detect candidate cycles)
	    - 3: done (collect cycles)
	 */
	int v_done = -1;
	typename t_slot<T_type>::t_increments v_increments;
	typename t_slot<T_type>::t_decrements v_decrements;
#if WIN32
	HANDLE v_handle;
#else
	pthread_t v_handle;
#endif
	std::unique_ptr<char[]> v_stack_buffer;
	t_object<T_type>** v_stack_last_top;
	//! Bottom of the copied stack of the last epoch.
	t_object<T_type>** v_stack_last_bottom;
	//! Bottom of the copied stack.
	t_object<T_type>** v_stack_copy;
	//! Bottom of the real stack.
	t_object<T_type>** v_stack_bottom;
	void* v_stack_limit;
	t_object<T_type>** v_stack_top;
	/*!
	  Point at which reviving occurred.
	  Until this point is processed, the collector postpones cycle collection if there is any object to be actually revived.
	 */
	t_object<T_type>* volatile* v_reviving = nullptr;

	void f_initialize(void* a_bottom);
	void f_epoch_get()
	{
		t_object<T_type>* dummy = nullptr;
		v_stack_top = &dummy;
		v_increments.v_epoch.store(v_increments.v_head, std::memory_order_release);
		v_decrements.v_epoch.store(v_decrements.v_head, std::memory_order_release);
	}
	void f_epoch_suspend()
	{
#if WIN32
		SuspendThread(v_handle);
		f_epoch_get();
#else
		f_engine<T_type>()->f_epoch_send(v_handle, SIGUSR1);
#endif
	}
	void f_epoch_resume()
	{
#if WIN32
		ResumeThread(v_handle);
#else
		f_engine<T_type>()->f_epoch_send(v_handle, SIGUSR2);
#endif
	}
	void f_epoch();
	void f_revive()
	{
		v_reviving = v_increments.v_head;
	}

public:
	static t_thread* f_current()
	{
		return v_current;
	}

	/*!
	  \sa t_engine::f_exit skips waiting for this thread to finish if true.
	  Must be accessed with t_engine::v_thread__mutex locked.
	 */
	bool v_background = false;

	t_thread();
	/*!
	  Gets the running status of the thread.
	  \return
	    - <0: not started
	    - 0: running
	    - >0: done
	 */
	int f_done() const
	{
		return v_done;
	}
	//! Gets the native handle.
	auto f_handle() const
	{
		return v_handle;
	}
	//! Gets whether \p a_p is on the stack of the thread.
	bool f_on_stack(void* a_p) const
	{
		return a_p >= v_stack_limit && a_p < static_cast<void*>(v_stack_bottom);
	}
};

template<typename T_type>
void t_thread<T_type>::f_initialize(void* a_bottom)
{
#if WIN32
	v_handle = GetCurrentThread();
#else
	v_handle = pthread_self();
#endif
	v_stack_bottom = reinterpret_cast<t_object<T_type>**>(a_bottom);
	auto page = sysconf(_SC_PAGESIZE);
	rlimit limit;
	if (getrlimit(RLIMIT_STACK, &limit) == -1) throw std::system_error(errno, std::generic_category());
	v_stack_limit = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(a_bottom) / page * page + page - limit.rlim_cur);
	v_current = this;
	t_slot<T_type>::t_increments::v_instance = &v_increments;
	t_slot<T_type>::t_increments::v_head = v_increments.v_objects;
	t_slot<T_type>::t_increments::v_next = v_increments.v_objects + t_slot<T_type>::t_increments::V_SIZE / 8;
	t_slot<T_type>::t_decrements::v_instance = &v_decrements;
	t_slot<T_type>::t_decrements::v_head = v_decrements.v_objects;
	t_slot<T_type>::t_decrements::v_next = v_decrements.v_objects + t_slot<T_type>::t_decrements::V_SIZE / 8;
	v_done = 0;
}

template<typename T_type>
void t_thread<T_type>::f_epoch()
{
	auto top0 = v_stack_copy;
	auto top1 = v_stack_last_bottom;
	if (v_done > 0) {
		++v_done;
	} else {
		f_epoch_suspend();
		auto n = v_stack_bottom - v_stack_top;
		top0 -= n;
		std::memcpy(top0, v_stack_top, n * sizeof(t_object<T_type>*));
		f_epoch_resume();
		top1 -= n;
	}
	auto decrements = v_stack_last_bottom;
	{
		auto top2 = v_stack_last_top;
		v_stack_last_top = top1;
		std::lock_guard lock(f_engine<T_type>()->v_object__heap.f_mutex());
		if (top1 < top2) {
			do {
				auto p = f_engine<T_type>()->f_object__find(*top0++);
				if (p) p->f_increment();
				*top1++ = p;
			} while (top1 < top2);
		} else {
			for (; top2 < top1; ++top2) if (*top2) *decrements++ = *top2;
		}
		for (; top0 < v_stack_copy; ++top1) {
			auto p = *top0++;
			auto q = *top1;
			if (p == q) continue;
			p = f_engine<T_type>()->f_object__find(p);
			if (p == q) continue;
			if (p) p->f_increment();
			if (q) *decrements++ = q;
			*top1 = p;
		}
	}
	v_increments.f_flush();
	for (auto p = v_stack_last_bottom; p != decrements; ++p) (*p)->f_decrement();
	v_decrements.f_flush();
}

template<typename T_type>
t_thread<T_type>::t_thread() : v_next(f_engine<T_type>()->v_thread__head)
{
	if (f_engine<T_type>()->v_exiting) throw std::runtime_error("engine is exiting.");
	rlimit limit;
	if (getrlimit(RLIMIT_STACK, &limit) == -1) throw std::system_error(errno, std::generic_category());
	v_stack_buffer = std::make_unique<char[]>(limit.rlim_cur * 2);
	auto p = v_stack_buffer.get() + limit.rlim_cur;
	v_stack_last_top = v_stack_last_bottom = reinterpret_cast<t_object<T_type>**>(p);
	v_stack_copy = reinterpret_cast<t_object<T_type>**>(p + limit.rlim_cur);
	f_engine<T_type>()->v_thread__head = this;
}

}

#endif
