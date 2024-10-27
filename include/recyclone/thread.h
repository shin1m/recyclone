#ifndef RECYCLONE__THREAD_H
#define RECYCLONE__THREAD_H

#include "object.h"
#include <thread>
#ifndef RECYCLONE__COOPERATIVE
#include <csignal>
#endif
#include <cstring>
#ifdef RECYCLONE__COOPERATIVE
#ifdef __unix__
#ifndef __EMSCRIPTEN__
#include <ucontext.h>
#endif
#endif
#endif

namespace recyclone
{

template<typename T_type>
void f_epoch_point();
template<typename T_type>
auto f_epoch_region(auto);

//! A set of thread data for garbage collection.
template<typename T_type>
class t_thread
{
	friend class t_engine<T_type>;
	friend void f_epoch_point<T_type>();
	template<typename> friend auto f_epoch_region(auto);

	static inline RECYCLONE__THREAD t_thread* v_current;

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
#ifdef RECYCLONE__COOPERATIVE
	/*!
	  Epoch suspension status and poll function for epoch point:
	    - f_epoch_off: no suspension is requested.
	    - f_epoch_on: suspension is requested or the thread is in epoch region.
	    - nullptr: suspension is in progress.
	 */
	std::atomic<void(*)()> v_epoch__poll = f_epoch_off;
	/*!
	  Place of suspension:
	    - f_epoch_off: epoch point
	    - f_epoch_on: epoch region
	 */
	void(*v_epoch__at)();
#endif
#ifdef __unix__
	pthread_t v_handle;
#endif
#ifdef _WIN32
	HANDLE v_handle = NULL;
#ifndef RECYCLONE__COOPERATIVE
	CONTEXT v_context_last{};
	CONTEXT v_context{};
#endif
#endif
	//! Copied stack of the last epoch.
	std::unique_ptr<t_object<T_type>*[]> v_stack_last;
	size_t v_stack_last_size;
	//! Top of the copied stack of the last epoch.
	t_object<T_type>** v_stack_last_top;
	//! Bottom of the real stack.
	t_object<T_type>** v_stack_bottom;
	//! Top limit of the real stack.
	void* v_stack_limit;
	//! Current top of the real stack.
	t_object<T_type>** v_stack_top;

	t_thread() : v_next(f_engine<T_type>()->v_thread__head)
	{
		f_engine<T_type>()->v_thread__head = this;
	}
#ifdef _WIN32
	~t_thread()
	{
		if (v_handle != NULL) CloseHandle(v_handle);
	}
#endif
	void f_initialize(void* a_bottom);
	void f_epoch__get()
	{
		v_increments.v_epoch.store(v_increments.v_head, std::memory_order_release);
		v_decrements.v_epoch.store(v_decrements.v_head, std::memory_order_release);
	}
#ifdef RECYCLONE__COOPERATIVE
	auto f_epoch_get(auto a_do)
	{
#ifdef __unix__
#ifdef __EMSCRIPTEN__
		t_object<T_type>* context = nullptr;
#else
		ucontext_t context;
		getcontext(&context);
#endif
#endif
#ifdef _WIN32
		CONTEXT context{};
		context.ContextFlags = CONTEXT_INTEGER;
		GetThreadContext(v_handle, &context);
#endif
		v_stack_top = reinterpret_cast<t_object<T_type>**>(&context);
		f_epoch__get();
		return a_do();
	}
	static void f_epoch_off()
	{
	}
	void f_epoch_sleep()
	{
		v_epoch__poll.store(nullptr, std::memory_order_release);
		v_epoch__poll.notify_one();
		v_epoch__poll.wait(nullptr, std::memory_order_relaxed);
	}
	static void f_epoch_on()
	{
		v_current->f_epoch_get([]
		{
			v_current->f_epoch_sleep();
		});
	}
	void f_epoch_suspend()
	{
		while (true) {
			v_epoch__at = f_epoch_off;
			if (v_epoch__poll.compare_exchange_strong(v_epoch__at, f_epoch_on, std::memory_order_relaxed, std::memory_order_relaxed)) {
				v_epoch__poll.wait(f_epoch_on, std::memory_order_acquire);
				break;
			}
			v_epoch__at = f_epoch_on;
			if (v_epoch__poll.compare_exchange_strong(v_epoch__at, nullptr, std::memory_order_acquire, std::memory_order_relaxed)) break;
		}
	}
	void f_epoch_resume()
	{
		v_epoch__poll.store(v_epoch__at, std::memory_order_relaxed);
		v_epoch__poll.notify_one();
	}
#else
	void f_epoch_get()
	{
		t_object<T_type>* dummy = nullptr;
		v_stack_top = &dummy;
		f_epoch__get();
	}
	void f_epoch_suspend()
	{
#ifdef __unix__
		pthread_kill(v_handle, RECYCLONE__SIGNAL_SUSPEND);
		f_engine<T_type>()->f_epoch_wait();
#endif
#ifdef _WIN32
		SuspendThread(v_handle);
		GetThreadContext(v_handle, &v_context);
		v_stack_top = reinterpret_cast<t_object<T_type>**>(v_context.Rsp);
		MEMORY_BASIC_INFORMATION mbi;
		for (auto p = v_stack_top;;) {
			VirtualQuery(p, &mbi, sizeof(mbi));
			p = reinterpret_cast<t_object<T_type>**>(static_cast<char*>(mbi.BaseAddress) + mbi.RegionSize);
			if (mbi.Protect & PAGE_GUARD) {
				v_stack_top = p;
				break;
			}
			if (p >= v_stack_bottom) break;
		}
		v_increments.v_epoch.store(v_increments.v_head, std::memory_order_relaxed);
		v_decrements.v_epoch.store(v_decrements.v_head, std::memory_order_relaxed);
#endif
	}
	void f_epoch_resume()
	{
#ifdef __unix__
		pthread_kill(v_handle, RECYCLONE__SIGNAL_RESUME);
#endif
#ifdef _WIN32
		ResumeThread(v_handle);
#endif
	}
	void f_epoch_done()
	{
		f_epoch__get();
	}
#endif
	void f_epoch();

public:
	static t_thread* f_current()
	{
		return v_current;
	}

	/*!
	  \sa t_engine::f_join_foregrounds skips waiting for this thread to finish if true.
	  Must be accessed with t_engine::v_thread__mutex locked.
	 */
	bool v_background = false;

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
#ifdef RECYCLONE__COOPERATIVE
	RECYCLONE__NOINLINE void f_epoch_enter();
	RECYCLONE__NOINLINE void f_epoch_leave();
	RECYCLONE__NOINLINE void f_epoch_done();
#endif
};

template<typename T_type>
void t_thread<T_type>::f_initialize(void* a_bottom)
{
	v_stack_bottom = static_cast<t_object<T_type>**>(a_bottom);
#ifdef __unix__
	v_handle = pthread_self();
	pthread_attr_t a;
	if (auto error = pthread_getattr_np(v_handle, &a)) throw std::system_error(error, std::generic_category());
	size_t size;
	if (auto error = pthread_attr_getstack(&a, &v_stack_limit, &size)) throw std::system_error(error, std::generic_category());
	if (auto error = pthread_attr_destroy(&a)) throw std::system_error(error, std::generic_category());
#endif
#ifdef _WIN32
	DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &v_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
#ifndef RECYCLONE__COOPERATIVE
	v_context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
#endif
	ULONG_PTR low;
	ULONG_PTR high;
	GetCurrentThreadStackLimits(&low, &high);
	v_stack_limit = reinterpret_cast<void*>(low);
#endif
	v_stack_last_size = v_stack_bottom - static_cast<t_object<T_type>**>(v_stack_limit);
	v_stack_last.reset(new t_object<T_type>*[v_stack_last_size]);
	v_stack_last_top = v_stack_last.get() + v_stack_last_size;
	v_current = this;
	t_slot<T_type>::t_increments::v_instance = &v_increments;
	v_increments.v_head = v_increments.v_objects;
	t_slot<T_type>::t_increments::v_next = v_increments.v_objects + t_slot<T_type>::t_increments::c_SIZE / 8;
	t_slot<T_type>::t_decrements::v_instance = &v_decrements;
	v_decrements.v_head = v_decrements.v_objects;
	t_slot<T_type>::t_decrements::v_next = v_decrements.v_objects + t_slot<T_type>::t_decrements::c_SIZE / 8;
	v_done = 0;
}

template<typename T_type>
void t_thread<T_type>::f_epoch()
{
	auto top0 = f_engine<T_type>()->v_stack__copy.get() + f_engine<T_type>()->v_stack__copy_size;
	auto bottom1 = v_stack_last.get() + v_stack_last_size;
	auto top1 = bottom1;
	if (v_done > 0) {
		++v_done;
#ifdef _WIN32
#ifndef RECYCLONE__COOPERATIVE
		v_context = {};
#endif
#endif
	} else {
		f_epoch_suspend();
		auto n = v_stack_bottom - v_stack_top;
		top0 -= n;
		std::memcpy(top0, v_stack_top, n * sizeof(t_object<T_type>*));
		f_epoch_resume();
		top1 -= n;
	}
	auto decrements = f_engine<T_type>()->v_stack__copy.get();
#ifdef _WIN32
#ifndef RECYCLONE__COOPERATIVE
#ifndef NDEBUG
#pragma warning(push)
#pragma warning(disable : 4477)
	std::fprintf(stderr, "RAX: %p\nRCX: %p\nRDX: %p\nRBX: %p\nRSP: %p\nRBP: %p\nRSI: %p\nRDI: %p\nR8: %p\nR9: %p\nR10: %p\nR11: %p\nR12: %p\nR13: %p\nR14: %p\nR15: %p\n", v_context.Rax, v_context.Rcx, v_context.Rdx, v_context.Rbx, v_context.Rsp, v_context.Rbp, v_context.Rsi, v_context.Rdi, v_context.R8, v_context.R9, v_context.R10, v_context.R11, v_context.R12, v_context.R13, v_context.R14, v_context.R15);
#pragma warning(pop)
	for (size_t i = 0; i < sizeof(v_context) / sizeof(void*); ++i) std::fprintf(stderr, "%zu: %p\n", i, reinterpret_cast<void**>(&v_context)[i]);
#endif
	auto cdecrements = reinterpret_cast<t_object<T_type>**>(&v_context);
#endif
#endif
	{
		auto top2 = v_stack_last_top;
		v_stack_last_top = top1;
		std::lock_guard lock(f_engine<T_type>()->v_object__heap.f_mutex());
		if (top1 < top2)
			do {
				auto p = f_engine<T_type>()->f_object__find(*top0++);
				if (p) p->f_increment();
				*top1++ = p;
			} while (top1 < top2);
		else
			for (; top2 < top1; ++top2) if (*top2) *decrements++ = *top2;
#ifdef _WIN32
#ifndef RECYCLONE__COOPERATIVE
		auto increment = [&](auto top0, auto top1, auto bottom1, auto& decrements)
		{
#endif
#endif
		for (; top1 < bottom1; ++top1) {
			auto p = *top0++;
			auto q = *top1;
			if (p == q) continue;
			p = f_engine<T_type>()->f_object__find(p);
			if (p == q) continue;
			if (p) p->f_increment();
			if (q) *decrements++ = q;
			*top1 = p;
		}
#ifdef _WIN32
#ifndef RECYCLONE__COOPERATIVE
		};
		increment(top0, top1, bottom1, decrements);
		increment(reinterpret_cast<t_object<T_type>**>(&v_context), reinterpret_cast<t_object<T_type>**>(&v_context_last), reinterpret_cast<t_object<T_type>**>(&v_context_last + 1), cdecrements);
#endif
#endif
	}
	v_increments.f_flush();
	for (auto p = f_engine<T_type>()->v_stack__copy.get(); p != decrements; ++p) (*p)->f_decrement();
#ifdef _WIN32
#ifndef RECYCLONE__COOPERATIVE
	for (auto p = reinterpret_cast<t_object<T_type>**>(&v_context); p != cdecrements; ++p) (*p)->f_decrement();
#endif
#endif
	v_decrements.f_flush();
}

#ifdef RECYCLONE__COOPERATIVE
template<typename T_type>
void t_thread<T_type>::f_epoch_enter()
{
	while (true) {
		auto p = f_epoch_off;
		if (v_epoch__poll.compare_exchange_strong(p, f_epoch_on, std::memory_order_release, std::memory_order_relaxed)) break;
		f_epoch_sleep();
	}
}

template<typename T_type>
void t_thread<T_type>::f_epoch_leave()
{
	while (true) {
		auto p = f_epoch_on;
		if (v_epoch__poll.compare_exchange_strong(p, f_epoch_off, std::memory_order_relaxed, std::memory_order_relaxed)) break;
		v_epoch__poll.wait(p, std::memory_order_relaxed);
	}
}

template<typename T_type>
void t_thread<T_type>::f_epoch_done()
{
	f_epoch_get([this]
	{
		f_epoch_enter();
	});
}

template<typename T_type>
struct t_epoch_region
{
	t_epoch_region()
	{
		t_thread<T_type>::f_current()->f_epoch_enter();
	}
	~t_epoch_region()
	{
		t_thread<T_type>::f_current()->f_epoch_leave();
	}
};

template<typename T_type>
struct t_epoch_noiger
{
	t_epoch_noiger()
	{
		t_thread<T_type>::f_current()->f_epoch_leave();
	}
	~t_epoch_noiger()
	{
		t_thread<T_type>::f_current()->f_epoch_done();
	}
};
#endif

//! Polls epoch suspension request.
/*!
  This should be called periodically in order for GC to keep working.
  Example places:
    - Beginnings of functions.
    - Back edges of loops.
    - Catch handlers.
 */
template<typename T_type>
inline void f_epoch_point()
{
#ifdef RECYCLONE__COOPERATIVE
	t_thread<T_type>::v_current->v_epoch__poll.load(std::memory_order_relaxed)();
#endif
}

#ifdef RECYCLONE__COOPERATIVE
//! Establishes a region where epoch suspension is allowed.
/*!
  For each blocking operation, this should be placed surrounding it.
  The object graph must not be manipulated inside the region.
 */
template<typename T_type>
RECYCLONE__NOINLINE auto f_epoch_region(auto a_do)
{
	return t_thread<T_type>::v_current->f_epoch_get([&]
	{
		t_epoch_region<T_type> region;
		return a_do();
	});
}
//! The opposite to \sa f_epoch_region.
/*!
  Inside the epoch region, this establishes an inverse region inside which the object graph can be manipulated.
 */
template<typename T_type>
RECYCLONE__NOINLINE auto f_epoch_noiger(auto a_do)
{
	t_epoch_noiger<T_type> noiger;
	return a_do();
}
#else
template<typename T_type>
inline auto f_epoch_region(auto a_do)
{
	return a_do();
}
template<typename T_type>
inline auto f_epoch_noiger(auto a_do)
{
	return a_do();
}
#endif

}

#endif
