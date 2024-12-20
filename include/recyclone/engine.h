#ifndef RECYCLONE__ENGINE_H
#define RECYCLONE__ENGINE_H

#include "thread.h"
#include <condition_variable>
#include <deque>
#ifndef RECYCLONE__COOPERATIVE
#ifdef __unix__
#include <semaphore.h>
#endif
#endif

namespace recyclone
{

template<typename T_type>
class t_engine
{
	friend class t_object<T_type>;
	friend class t_thread<T_type>;
	friend t_engine* f_engine<T_type>();

	struct t_conductor
	{
		std::atomic_flag v_running;
		bool v_quitting = false;

		t_conductor()
		{
			v_running.test_and_set(std::memory_order_relaxed);
		}
		void f_next()
		{
			v_running.clear(std::memory_order_relaxed);
			v_running.notify_all();
			v_running.wait(false, std::memory_order_acquire);
		}
		void f_exit()
		{
			v_running.clear(std::memory_order_relaxed);
			v_running.notify_one();
		}
		void f_wake()
		{
			if (!v_running.test_and_set(std::memory_order_release)) v_running.notify_one();
		}
		void f_run()
		{
			f_wake();
			v_running.wait(true, std::memory_order_relaxed);
		}
		void f_quit()
		{
			v_quitting = true;
			f_run();
		}
	};
	//! The longest case after losing the last reference is missing an epoch -> skipping an epoch -> collected as a candidate cycle -> false reviving -> freed as a cycle.
	static constexpr size_t c_COLLECTION_WAIT_COUNT = 5;

protected:
	static inline RECYCLONE__THREAD t_engine* v_instance;

	t_conductor v_collector__conductor;
	std::atomic_size_t v_collector__tick;
	std::atomic_size_t v_collector__wait;
	size_t v_collector__epoch = 0;
	size_t v_collector__collect = 0;
	std::atomic_size_t v_collector__full;
	t_object<T_type>* v_cycles = nullptr;
	t_heap<t_object<T_type>> v_object__heap;
	size_t v_object__lower = 0;
	std::mutex v_object__reviving__mutex;
	size_t v_object__release = 0;
	size_t v_object__collect = 0;
#ifndef RECYCLONE__COOPERATIVE
#ifdef __unix__
	sem_t v_epoch__received;
	sigset_t v_epoch__not_signal_resume;
	struct sigaction v_epoch__old_signal_suspend;
	struct sigaction v_epoch__old_signal_resume;
#endif
#endif
	std::unique_ptr<t_object<T_type>*[]> v_stack__copy;
	size_t v_stack__copy_size = 0;
	t_thread<T_type>* v_thread__head = nullptr;
	t_thread<T_type>* v_thread__main;
	t_thread<T_type>* v_thread__finalizer = nullptr;
	std::mutex v_thread__mutex;
	std::condition_variable v_thread__condition;
	t_conductor v_finalizer__conductor;
	std::mutex v_finalizer__mutex;
	std::deque<t_object<T_type>*> v_finalizer__queue;
	bool v_finalizer__sleeping = false;
	uint8_t v_finalizer__awaken = 0;
	bool v_exiting = false;

	void f_free(t_object<T_type>* a_p)
	{
		a_p->v_count = 1;
		v_object__heap.f_free(a_p);
	}
	void f_free_as_release(t_object<T_type>* a_p)
	{
		++v_object__release;
		f_free(a_p);
	}
	void f_free_as_collect(t_object<T_type>* a_p)
	{
		++v_object__collect;
		f_free(a_p);
	}
	t_object<T_type>* f_object__find(void* a_p)
	{
		if (reinterpret_cast<uintptr_t>(a_p) & t_heap<t_object<T_type>>::c_UNIT - 1) return nullptr;
		auto p = v_object__heap.f_find(a_p);
		return p && p->v_type ? p : nullptr;
	}
#ifndef RECYCLONE__COOPERATIVE
#ifdef __unix__
	void f_epoch_suspend()
	{
		if (sem_post(&v_epoch__received) == -1) _exit(errno);
		auto e = errno;
		sigsuspend(&v_epoch__not_signal_resume);
		errno = e;
	}
	void f_epoch_wait()
	{
		while (sem_wait(&v_epoch__received) == -1) if (errno != EINTR) throw std::system_error(errno, std::generic_category());
	}
#endif
#endif
	void f_collector();
	void f_finalizer(void(*a_finalize)(t_object<T_type>*));
	void f_finalize(t_thread<T_type>* a_thread);

public:
	struct t_options
	{
#ifdef NDEBUG
		size_t v_collector__threshold = 1024 * 64;
#else
		size_t v_collector__threshold = 64;
#endif
		bool v_verbose = false;
	};

	const t_options& v_options;

	t_engine(const t_options& a_options, void* a_bottom = nullptr);
	//! Tries to collect all the objects and detects leaks.
	~t_engine();
	//! Triggers garbage collection.
	void f_tick()
	{
		if (v_collector__conductor.v_running.test(std::memory_order_relaxed)) return;
		v_collector__tick.fetch_add(1, std::memory_order_relaxed);
		v_collector__conductor.f_wake();
	}
	//! Triggers garbage collection and waits for it to finish.
#ifdef RECYCLONE__COOPERATIVE
	void f_wait()
	{
		f_epoch_region<T_type>([this]
		{
			f__wait();
		});
	}
	void f__wait()
#else
	void f_wait()
#endif
	{
		v_collector__wait.fetch_add(1, std::memory_order_relaxed);
		v_collector__conductor.f_run();
	}
	//! Allocates a new object with the size \p a_size.
	RECYCLONE__ALWAYS_INLINE constexpr t_object<T_type>* f_allocate(size_t a_size)
	{
		auto p = v_object__heap.f_allocate(a_size);
		p->v_next = nullptr;
		return p;
	}
	//! Performs full garbage collection.
	void f_collect();
	//! Performs finalization.
	void f_finalize();
	//! Gets whether \sa f_join_foregrounds finished waiting foreground threads.
	bool f_exiting() const
	{
		return v_exiting;
	}
	//! Starts a new thread that calls \p a_main for \p a_thread.
	void f_start(auto* a_thread, auto a_initialize, auto a_main);
	//! Waits for \p a_thread to finish.
	void f_join(auto* a_thread);
	//! Waits for foreground threads to finish and mark exiting.
	void f_join_foregrounds();
	//! Quits finalizer.
	void f_quit_finalizer();
	/*!
	  Performs an operation for reviving objects, synchronizing with the free cycles.
	  \param a_do An operation which is called with an operation to mark an object reviving.
	  How to make an object revivable:
	    - Make the object cyclic.
	    - Mark the object reviving in \p a_do.
	  Do not perform \sa t_slot operations involving \sa t_slot::t_increments::f_push or \sa t_slot::t_decrements::f_push in \p a_do, since they cause deadlocks with the free cycles.
	 */
	auto f_revive(auto a_do)
	{
		std::lock_guard lock(v_object__reviving__mutex);
		return a_do([&](t_object<T_type>* a_p)
		{
			a_p->v_reviving = true;
		});
	}
};

template<typename T_type>
void t_engine<T_type>::f_collector()
{
	v_instance = this;
	if (v_options.v_verbose) std::fprintf(stderr, "collector starting...\n");
	t_object<T_type>::v_roots.v_next = t_object<T_type>::v_roots.v_previous = reinterpret_cast<t_object<T_type>*>(&t_object<T_type>::v_roots);
	while (true) {
		v_collector__conductor.f_next();
		if (v_collector__conductor.v_quitting) break;
		++v_collector__epoch;
		{
			std::lock_guard lock(v_thread__mutex);
			size_t stack = 0;
			for (auto p = v_thread__head; p; p = p->v_next) if (p->v_done >= 0 && p->v_stack_last_size > stack) stack = p->v_stack_last_size;
			if (stack != v_stack__copy_size) {
				v_stack__copy.reset(new t_object<T_type>*[stack]);
				v_stack__copy_size = stack;
			}
			for (auto p = &v_thread__head; *p;) {
				auto q = *p;
				auto active = q->v_done >= 0;
				if (active && q == v_thread__finalizer && q->v_done <= 0) {
					active = v_finalizer__awaken;
					if (active && v_finalizer__sleeping) --v_finalizer__awaken;
				}
				if (active) q->f_epoch();
				if (q->v_done < 3) {
					p = &q->v_next;
				} else {
					*p = q->v_next;
					delete q;
				}
			}
		}
		t_object<T_type>* garbage = nullptr;
		while (v_cycles) {
			std::lock_guard lock(v_object__reviving__mutex);
			auto cycle = v_cycles;
			v_cycles = cycle->v_next_cycle;
			auto failed = false;
			auto finalizee = false;
			auto p = cycle;
			while (true) {
				auto q = p->v_next;
				if (q->v_type) {
					if (q->v_color != c_color__ORANGE || q->v_cyclic > 0 || q->v_reviving) failed = true;
					q->v_reviving = false;
					if (q->v_finalizee) finalizee = true;
					p = q;
					if (p == cycle) break;
				} else {
					p->v_next = q->v_next;
					f_free_as_collect(q);
					if (q == cycle) {
						cycle = p == q ? nullptr : p;
						break;
					}
				}
			}
			if (!cycle) continue;
			if (failed) {
				p = cycle;
				if (p->v_color == c_color__ORANGE) p->v_color = c_color__PURPLE;
				do {
					auto q = p->v_next;
					if (p->v_count <= 0) {
						p->v_next = garbage;
						garbage = p;
					} else if (p->v_color == c_color__PURPLE) {
						t_object<T_type>::f_append(p);
					} else {
						p->v_color = c_color__BLACK;
						p->v_next = nullptr;
					}
					p = q;
				} while (p != cycle);
			} else {
				if (finalizee) {
					std::lock_guard lock(v_finalizer__mutex);
					if (v_finalizer__conductor.v_quitting) {
						finalizee = false;
					} else {
						do {
							p->v_type->f_prepare_for_finalizer(p);
							auto q = p->v_next;
							if (p->v_finalizee) {
								p->f_increment();
								p->v_next = nullptr;
								v_finalizer__queue.push_back(p);
							} else {
								p->v_color = c_color__PURPLE;
								t_object<T_type>::f_append(p);
							}
							p = q;
						} while (p != cycle);
						v_finalizer__conductor.f_wake();
					}
				}
				if (!finalizee) {
					do p->v_color = c_color__RED; while ((p = p->v_next) != cycle);
					do p->f_cyclic_decrement(); while ((p = p->v_next) != cycle);
					do {
						auto q = p->v_next;
						f_free_as_collect(p);
						p = q;
					} while (p != cycle);
				}
			}
		}
		while (garbage) {
			auto p = garbage;
			garbage = p->v_next;
			p->v_next = nullptr;
			if (!p->v_finalizee || !p->f_queue_finalize()) p->template f_loop<&t_object<T_type>::f_decrement_step>();
		}
		auto roots = reinterpret_cast<t_object<T_type>*>(&t_object<T_type>::v_roots);
		if (roots->v_next != roots) {
			auto live = v_object__heap.f_live();
			if (live < v_object__lower) v_object__lower = live;
			if (v_collector__full.load(std::memory_order_relaxed) > 0 || live - v_object__lower >= v_options.v_collector__threshold) {
				v_object__lower = live;
				++v_collector__collect;
				{
					auto p = roots;
					auto q = p->v_next;
					do {
						assert(q->v_count > 0);
						if (q->v_color == c_color__PURPLE) {
							q->f_mark_gray();
							p = q;
						} else {
							p->v_next = q->v_next;
							q->v_next = nullptr;
						}
						q = p->v_next;
					} while (q != roots);
				}
				if (roots->v_next != roots) {
					{
						auto p = roots->v_next;
						do p->f_scan_gray(); while ((p = p->v_next) != roots);
					}
					do {
						auto p = roots->v_next;
						roots->v_next = p->v_next;
						if (p->v_color == c_color__WHITE) {
							p->f_collect_white();
							auto cycle = t_object<T_type>::v_cycle;
							auto q = cycle;
							do q->template f_step<&t_object<T_type>::f_scan_red>(); while ((q = q->v_next) != cycle);
							do q->v_color = c_color__ORANGE; while ((q = q->v_next) != cycle);
							cycle->v_next_cycle = v_cycles;
							v_cycles = cycle;
						} else {
							p->v_next = nullptr;
						}
					} while (roots->v_next != roots);
				}
				roots->v_previous = roots;
			}
		}
		v_object__heap.f_flush();
	}
	if (v_options.v_verbose) std::fprintf(stderr, "collector quitting...\n");
	v_collector__conductor.f_exit();
}

void f_with_scratch(auto a_do)
{
	char padding[4096];
	std::memset(padding, 0, sizeof(padding));
	a_do();
	// TODO: Try to clear volatile registers.
	[](intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t) -> intptr_t
	{
		return 0;
	}(0, 0, 0, 0, 0, 0, 0, 0);
#ifdef __unix__
#ifdef __amd64__
	__asm__(
		"mov $0, %r10\n\t"
		"mov $0, %r11\n\t"
	);
#endif
#ifdef __aarch64__
	__asm__(
		"mov $0, %x9\n\t"
		"mov $0, %x10\n\t"
		"mov $0, %x11\n\t"
		"mov $0, %x12\n\t"
		"mov $0, %x13\n\t"
		"mov $0, %x14\n\t"
		"mov $0, %x15\n\t"
		"mov $0, %x16\n\t"
		"mov $0, %x17\n\t"
	);
#endif
#endif
#ifdef _WIN32
	CONTEXT context;
	context.ContextFlags = CONTEXT_INTEGER;
	auto thread = GetCurrentThread();
	GetThreadContext(thread, &context);
#ifdef _M_AMD64
	context.R10 = context.R11 = 0;
#endif
#ifdef _M_ARM64
	context.X9 = context.X10 = context.X11 = context.X12 = context.X13 = context.X14 = context.X15 = context.X16 = context.X17 = 0;
#endif
	SetThreadContext(thread, &context);
#endif
}

template<typename T_type>
void t_engine<T_type>::f_finalizer(void(*a_finalize)(t_object<T_type>*))
{
	if (v_options.v_verbose) std::fprintf(stderr, "finalizer starting...\n");
	while (true) {
		f_epoch_region<T_type>([this]
		{
			std::lock_guard lock(v_thread__mutex);
			v_finalizer__sleeping = true;
		});
		if (v_finalizer__conductor.v_quitting) break;
		f_epoch_region<T_type>([&, this]
		{
			v_finalizer__conductor.f_next();
		});
		f_epoch_region<T_type>([this]
		{
			std::lock_guard lock(v_thread__mutex);
			v_finalizer__sleeping = false;
			v_finalizer__awaken = 2;
		});
#ifndef NDEBUG
		f_with_scratch([&]
		{
#endif
		while (true) {
			t_object<T_type>* p;
			{
				std::lock_guard lock(v_finalizer__mutex);
				if (v_finalizer__queue.empty()) break;
				p = v_finalizer__queue.front();
				v_finalizer__queue.pop_front();
			}
			p->v_finalizee = false;
			a_finalize(p);
			t_slot<T_type>::t_decrements::f_push(p);
		}
#ifndef NDEBUG
		});
#endif
	}
	if (v_options.v_verbose) std::fprintf(stderr, "finalizer quitting...\n");
	v_finalizer__conductor.f_exit();
}

template<typename T_type>
void t_engine<T_type>::f_finalize(t_thread<T_type>* a_thread)
{
	v_object__heap.f_return();
	a_thread->f_epoch_done();
	std::lock_guard lock(v_thread__mutex);
	++a_thread->v_done;
	v_thread__condition.notify_all();
}

template<typename T_type>
t_engine<T_type>::t_engine(const t_options& a_options, void* a_bottom) : v_object__heap([]
{
	v_instance->f_tick();
}), v_options(a_options)
{
	v_instance = this;
#ifndef RECYCLONE__COOPERATIVE
#ifdef __unix__
	if (sem_init(&v_epoch__received, 0, 0) == -1) throw std::system_error(errno, std::generic_category());
	sigfillset(&v_epoch__not_signal_resume);
	sigdelset(&v_epoch__not_signal_resume, RECYCLONE__SIGNAL_RESUME);
	struct sigaction sa;
	sa.sa_handler = [](int)
	{
	};
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(RECYCLONE__SIGNAL_RESUME, &sa, &v_epoch__old_signal_resume) == -1) throw std::system_error(errno, std::generic_category());
	sa.sa_handler = [](int)
	{
		t_thread<T_type>::v_current->f_epoch_get();
		v_instance->f_epoch_suspend();
	};
	sigaddset(&sa.sa_mask, RECYCLONE__SIGNAL_RESUME);
	if (sigaction(RECYCLONE__SIGNAL_SUSPEND, &sa, &v_epoch__old_signal_suspend) == -1) throw std::system_error(errno, std::generic_category());
#endif
#endif
	v_thread__main = new t_thread<T_type>;
	v_thread__main->f_initialize(a_bottom ? a_bottom : this);
	std::thread(&t_engine::f_collector, this).detach();
}

template<typename T_type>
t_engine<T_type>::~t_engine()
{
	f_finalize(v_thread__main);
	v_collector__full.fetch_add(1, std::memory_order_relaxed);
#ifdef RECYCLONE__COOPERATIVE
	for (size_t i = 0; i < c_COLLECTION_WAIT_COUNT; ++i) f__wait();
#else
	for (size_t i = 0; i < c_COLLECTION_WAIT_COUNT; ++i) f_wait();
#endif
	v_collector__conductor.f_quit();
	assert(!v_thread__head);
#ifndef RECYCLONE__COOPERATIVE
#ifdef __unix__
	if (sem_destroy(&v_epoch__received) == -1) std::exit(errno);
	if (sigaction(RECYCLONE__SIGNAL_SUSPEND, &v_epoch__old_signal_suspend, NULL) == -1) std::exit(errno);
	if (sigaction(RECYCLONE__SIGNAL_RESUME, &v_epoch__old_signal_resume, NULL) == -1) std::exit(errno);
#endif
#endif
	if (v_options.v_verbose) std::fprintf(stderr, "statistics:\n\tt_object:\n");
	size_t allocated = 0;
	size_t freed = 0;
	v_object__heap.f_statistics([&](auto a_rank, auto a_grown, auto a_allocated, auto a_freed)
	{
		if (v_options.v_verbose) std::fprintf(stderr, "\t\trank%zu: %zu: %zu - %zu = %zu\n", a_rank, a_grown, a_allocated, a_freed, a_allocated - a_freed);
		allocated += a_allocated;
		freed += a_freed;
	});
	if (v_options.v_verbose) {
		std::fprintf(stderr, "\t\ttotal: %zu - %zu = %zu, release = %zu, collect = %zu\n", allocated, freed, allocated - freed, v_object__release, v_object__collect);
		std::fprintf(stderr, "\tcollector: tick = %zu, wait = %zu, epoch = %zu, collect = %zu\n", v_collector__tick.load(std::memory_order_relaxed), v_collector__wait.load(std::memory_order_relaxed), v_collector__epoch, v_collector__collect);
	}
	if (allocated <= freed) return;
	if (v_options.v_verbose) {
		std::map<T_type*, size_t> leaks;
		for (auto& x : v_object__heap.f_blocks())
			if (x.first->v_rank < 7) {
				auto p0 = reinterpret_cast<char*>(x.first);
				auto p1 = p0 + x.second;
				auto unit = 128 << x.first->v_rank;
				for (; p0 < p1; p0 += unit) {
					auto p = reinterpret_cast<t_object<T_type>*>(p0);
					if (p->v_type) ++leaks[p->v_type];
				}
			} else {
				++leaks[x.first->v_type];
			}
		for (const auto& x : leaks) std::fprintf(stderr, "%p: %zu\n", x.first, x.second);
	}
	std::terminate();
}

template<typename T_type>
void t_engine<T_type>::f_collect()
{
	v_collector__full.fetch_add(1, std::memory_order_relaxed);
	for (size_t i = 0; i < c_COLLECTION_WAIT_COUNT; ++i) f_wait();
	v_collector__full.fetch_sub(1, std::memory_order_relaxed);
}

template<typename T_type>
void t_engine<T_type>::f_finalize()
{
	f_epoch_region<T_type>([this]
	{
		for (size_t i = 0; i < 2; ++i) v_finalizer__conductor.f_run();
	});
}

template<typename T_type>
void t_engine<T_type>::f_start(auto* a_thread, auto a_initialize, auto a_main)
{
	// t_thread must be chained first in order to synchronize with f_exit().
	f_epoch_region<T_type>([&, this]
	{
		std::lock_guard lock(v_thread__mutex);
		if (v_exiting) throw std::runtime_error("engine is exiting.");
		if (a_thread->v_internal) throw std::runtime_error("already started.");
		a_thread->v_internal = new t_thread<T_type>;
	});
	t_slot<T_type>::t_increments::f_push(a_thread);
	try {
		std::thread([this, a_thread, initialize = std::move(a_initialize), main = std::move(a_main)]
		{
			v_instance = this;
			auto internal = a_thread->v_internal;
			{
				std::lock_guard lock(v_thread__mutex);
				internal->f_initialize(&internal);
				initialize();
				if (internal->v_background) v_thread__condition.notify_all();
			}
			main();
			f_epoch_region<T_type>([&, this]
			{
				std::lock_guard lock(v_thread__mutex);
				internal->v_background = false;
				a_thread->v_internal = nullptr;
			});
			t_slot<T_type>::t_decrements::f_push(a_thread);
			f_finalize(internal);
		}).detach();
	} catch (...) {
		f_epoch_region<T_type>([&, this]
		{
			std::lock_guard lock(v_thread__mutex);
			a_thread->v_internal->v_done = 1;
			a_thread->v_internal = nullptr;
			v_thread__condition.notify_all();
		});
		t_slot<T_type>::t_decrements::f_push(a_thread);
		throw;
	}
}

template<typename T_type>
void t_engine<T_type>::f_join(auto* a_thread)
{
	if (a_thread->v_internal == t_thread<T_type>::v_current) throw std::runtime_error("current thread can not be joined.");
	if (a_thread->v_internal == v_thread__main) throw std::runtime_error("engine thread can not be joined.");
	f_epoch_region<T_type>([&, this]
	{
		std::unique_lock lock(v_thread__mutex);
		while (a_thread->v_internal) v_thread__condition.wait(lock);
	});
}

template<typename T_type>
void t_engine<T_type>::f_join_foregrounds()
{
	f_epoch_region<T_type>([this]
	{
		std::unique_lock lock(v_thread__mutex);
		auto tail = v_thread__finalizer ? v_thread__finalizer : v_thread__main;
		while (true) {
			auto p = v_thread__head;
			while (p != tail && (p->v_done > 0 || p->v_background)) p = p->v_next;
			if (p == tail) break;
			v_thread__condition.wait(lock);
		}
		v_exiting = true;
	});
}

template<typename T_type>
void t_engine<T_type>::f_quit_finalizer()
{
	f_collect();
	f_finalize();
	assert(v_thread__head == v_thread__finalizer);
	f_epoch_region<T_type>([this]
	{
		v_finalizer__conductor.f_quit();
		std::unique_lock lock(v_thread__mutex);
		while (v_thread__head->v_next && v_thread__head->v_done <= 0) v_thread__condition.wait(lock);
	});
}

template<typename T_type>
inline t_engine<T_type>* f_engine()
{
	return t_engine<T_type>::v_instance;
}

}

#endif
