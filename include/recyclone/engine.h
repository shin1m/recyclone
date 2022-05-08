#ifndef RECYCLONE__ENGINE_H
#define RECYCLONE__ENGINE_H

#include "thread.h"
#include "extension.h"
#include <deque>
#ifndef RECYCLONE__COOPERATIVE
#ifdef __unix__
#include <semaphore.h>
#endif
#endif

namespace recyclone
{

template<typename T_type>
t_engine<T_type>* f_engine();

template<typename T_type>
class t_engine
{
	friend class t_object<T_type>;
	friend class t_weak_pointer<T_type>;
	friend class t_thread<T_type>;
	friend t_engine* f_engine<T_type>();

	struct t_conductor
	{
		bool v_running = true;
		bool v_quitting = false;
		std::mutex v_mutex;
		std::condition_variable v_wake;
		std::condition_variable v_done;

		void f_next(std::unique_lock<std::mutex>& a_lock)
		{
			v_running = false;
			v_done.notify_all();
			do v_wake.wait(a_lock); while (!v_running);
		}
		void f_exit()
		{
			std::lock_guard lock(v_mutex);
			v_running = false;
			v_done.notify_one();
		}
		void f_wake()
		{
			if (v_running) return;
			v_running = true;
			v_wake.notify_one();
		}
		void f_wait(std::unique_lock<std::mutex>& a_lock)
		{
			do v_done.wait(a_lock); while (v_running);
		}
		void f_quit()
		{
			std::unique_lock lock(v_mutex);
			v_running = v_quitting = true;
			v_wake.notify_one();
			f_wait(lock);
		}
	};

protected:
	static inline RECYCLONE__THREAD t_engine* v_instance;

	t_conductor v_collector__conductor;
	size_t v_collector__threshold;
	size_t v_collector__tick = 0;
	size_t v_collector__wait = 0;
	size_t v_collector__epoch = 0;
	size_t v_collector__collect = 0;
	size_t v_collector__full = 0;
	t_object<T_type>* v_cycles = nullptr;
	t_heap<t_object<T_type>> v_object__heap;
	size_t v_object__lower = 0;
	bool v_object__reviving = false;
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
	t_thread<T_type>* v_thread__head = nullptr;
	t_thread<T_type>* v_thread__main;
	t_thread<T_type>* v_thread__finalizer = nullptr;
	std::mutex v_thread__mutex;
	std::condition_variable v_thread__condition;
	t_conductor v_finalizer__conductor;
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
		if (reinterpret_cast<uintptr_t>(a_p) & t_heap<t_object<T_type>>::V_UNIT - 1) return nullptr;
		auto p = v_object__heap.f_find(a_p);
		return p && p->v_type ? p : nullptr;
	}
#ifndef RECYCLONE__COOPERATIVE
#ifdef __unix__
	void f_epoch_suspend()
	{
		if (sem_post(&v_epoch__received) == -1) _exit(errno);
		sigsuspend(&v_epoch__not_signal_resume);
	}
	void f_epoch_wait()
	{
		while (sem_wait(&v_epoch__received) == -1) if (errno != EINTR) throw std::system_error(errno, std::generic_category());
	}
#endif
#endif
	void f_collector();
	void f_finalizer(void(*a_finalize)(t_object<T_type>*));
	size_t f_statistics();

public:
	struct t_options
	{
#ifdef NDEBUG
		size_t v_collector__threshold = 1024 * 64;
#else
		size_t v_collector__threshold = 64;
#endif
		bool v_verbose = false;
		bool v_verify = false;
	};

	const t_options& v_options;

	t_engine(const t_options& a_options, void* a_bottom = nullptr);
	~t_engine();
	/*!
	  Performs the exit sequence:
	  - Waits for foreground threads to finish.
	  - If \sa t_options::v_verify is true,
	    - Tries to collect all the objects and detects leaks.
	    - Returns \p a_code.
	  - Otherwise, immediately calls \sa std::exit with \p a_code.
	  \param a_code The exit code.
	  \return \p a_code.
	 */
	int f_exit(int a_code);
	//! Triggers garbage collection.
	void f_tick()
	{
		if (v_collector__conductor.v_running) return;
		std::lock_guard lock(v_collector__conductor.v_mutex);
		++v_collector__tick;
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
		std::unique_lock lock(v_collector__conductor.v_mutex);
		++v_collector__wait;
		v_collector__conductor.f_wake();
		v_collector__conductor.f_wait(lock);
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
	//! Gets whether the exit sequence finished waiting foreground threads.
	bool f_exiting() const
	{
		return v_exiting;
	}
	//! Starts a new thread that calls \p a_main for \p a_thread.
	template<typename T_thread, typename T_initialize, typename T_main>
	void f_start(T_thread* a_thread, T_initialize a_initialize, T_main a_main);
	//! Waits for \p a_thread to finish.
	template<typename T_thread>
	void f_join(T_thread* a_thread);
};

template<typename T_type>
void t_engine<T_type>::f_collector()
{
	v_instance = this;
	if (v_options.v_verbose) std::fprintf(stderr, "collector starting...\n");
	t_object<T_type>::v_roots.v_next = t_object<T_type>::v_roots.v_previous = reinterpret_cast<t_object<T_type>*>(&t_object<T_type>::v_roots);
	while (true) {
		{
			std::unique_lock lock(v_collector__conductor.v_mutex);
			v_collector__conductor.f_next(lock);
			if (v_collector__conductor.v_quitting) break;
		}
		++v_collector__epoch;
		{
			std::lock_guard lock(v_object__reviving__mutex);
			v_object__reviving = false;
		}
		{
			std::lock_guard lock(v_thread__mutex);
			for (auto p = &v_thread__head; *p;) {
				auto q = *p;
				auto active = q->v_done >= 0;
				if (active && q == v_thread__finalizer && q->v_done <= 0) {
					active = v_finalizer__awaken;
					if (active && v_finalizer__sleeping) --v_finalizer__awaken;
				}
				if (active) {
					auto tail = q->v_increments.v_tail;
					q->f_epoch();
					std::lock_guard lock(v_object__reviving__mutex);
					if (q->v_reviving) {
						size_t n = t_slot<T_type>::t_increments::V_SIZE;
						size_t epoch = (q->v_increments.v_tail + n - tail) % n;
						size_t reviving = (q->v_reviving + n - tail) % n;
						if (epoch < reviving)
							v_object__reviving = true;
						else
							q->v_reviving = nullptr;
					}
				}
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
					if (q->v_color != e_color__ORANGE || q->v_cyclic > 0)
						failed = true;
					else if (v_object__reviving)
					       if (auto p = q->v_extension.load(std::memory_order_relaxed)) if (p->v_weak_pointers__cycle) failed = true;
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
				if (p->v_color == e_color__ORANGE) p->v_color = e_color__PURPLE;
				do {
					auto q = p->v_next;
					if (p->v_count <= 0) {
						p->v_next = garbage;
						garbage = p;
					} else if (p->v_color == e_color__PURPLE) {
						t_object<T_type>::f_append(p);
					} else {
						p->v_color = e_color__BLACK;
						p->v_next = nullptr;
					}
					p = q;
				} while (p != cycle);
			} else {
				if (finalizee) {
					std::lock_guard lock(v_finalizer__conductor.v_mutex);
					if (v_finalizer__conductor.v_quitting) {
						finalizee = false;
					} else {
						do {
							if (auto q = p->v_extension.load(std::memory_order_relaxed)) q->f_detach();
							auto q = p->v_next;
							if (p->v_finalizee) {
								p->f_increment();
								p->v_next = nullptr;
								v_finalizer__queue.push_back(p);
							} else {
								p->v_color = e_color__PURPLE;
								t_object<T_type>::f_append(p);
							}
							p = q;
						} while (p != cycle);
						v_finalizer__conductor.f_wake();
					}
				}
				if (!finalizee) {
					do p->v_color = e_color__RED; while ((p = p->v_next) != cycle);
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
			if (live - v_object__lower >= v_collector__threshold) {
				v_object__lower = live;
				++v_collector__collect;
				{
					auto p = roots;
					auto q = p->v_next;
					do {
						assert(q->v_count > 0);
						if (q->v_color == e_color__PURPLE) {
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
						if (p->v_color == e_color__WHITE) {
							p->f_collect_white();
							auto cycle = t_object<T_type>::v_cycle;
							auto q = cycle;
							do q->template f_step<&t_object<T_type>::f_scan_red>(); while ((q = q->v_next) != cycle);
							do q->v_color = e_color__ORANGE; while ((q = q->v_next) != cycle);
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
		{
			std::unique_lock lock(v_finalizer__conductor.v_mutex);
			if (v_finalizer__conductor.v_quitting) break;
			f_epoch_region<T_type>([&, this]
			{
				v_finalizer__conductor.f_next(lock);
			});
		}
		f_epoch_region<T_type>([this]
		{
			std::lock_guard lock(v_thread__mutex);
			v_finalizer__sleeping = false;
			v_finalizer__awaken = 2;
		});
#ifndef NDEBUG
		[this, a_finalize]
		{
		char padding[4096];
		std::memset(padding, 0, sizeof(padding));
		[this, a_finalize]
		{
#endif
		while (true) {
			t_object<T_type>* p;
			{
				std::lock_guard lock(v_finalizer__conductor.v_mutex);
				if (v_finalizer__queue.empty()) break;
				p = v_finalizer__queue.front();
				v_finalizer__queue.pop_front();
			}
			p->v_finalizee = false;
			a_finalize(p);
			t_slot<T_type>::t_decrements::f_push(p);
		}
#ifndef NDEBUG
		}();
		}();
#endif
	}
	if (v_options.v_verbose) std::fprintf(stderr, "finalizer quitting...\n");
	v_finalizer__conductor.f_exit();
}

template<typename T_type>
size_t t_engine<T_type>::f_statistics()
{
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
		std::fprintf(stderr, "\tcollector: tick = %zu, wait = %zu, epoch = %zu, collect = %zu\n", v_collector__tick, v_collector__wait, v_collector__epoch, v_collector__collect);
	}
	return allocated - freed;
}

template<typename T_type>
t_engine<T_type>::t_engine(const t_options& a_options, void* a_bottom) : v_collector__threshold(a_options.v_collector__threshold), v_object__heap([]
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
	{
		v_thread__main->f_epoch_done();
		std::lock_guard lock(v_thread__mutex);
		++v_thread__main->v_done;
	}
#ifdef RECYCLONE__COOPERATIVE
	f__wait();
	f__wait();
	f__wait();
	f__wait();
#else
	f_wait();
	f_wait();
	f_wait();
	f_wait();
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
	if (f_statistics() <= 0) return;
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
int t_engine<T_type>::f_exit(int a_code)
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
	if (v_options.v_verify) {
		v_object__heap.f_return();
		{
			std::lock_guard lock(v_collector__conductor.v_mutex);
			if (v_collector__full++ <= 0) v_collector__threshold = 0;
		}
		if (!v_thread__finalizer) return a_code;
		f_wait();
		f_wait();
		f_wait();
		f_wait();
		f_finalize();
		assert(v_thread__head == v_thread__finalizer);
		f_epoch_region<T_type>([this]
		{
			v_finalizer__conductor.f_quit();
			std::unique_lock lock(v_thread__mutex);
			while (v_thread__head->v_next && v_thread__head->v_done <= 0) v_thread__condition.wait(lock);
		});
		return a_code;
	} else {
		if (v_options.v_verbose) f_statistics();
		std::exit(a_code);
	}
}

template<typename T_type>
void t_engine<T_type>::f_collect()
{
	{
		std::lock_guard lock(v_collector__conductor.v_mutex);
		if (v_collector__full++ <= 0) v_collector__threshold = 0;
	}
	f_wait();
	f_wait();
	f_wait();
	f_wait();
	{
		std::lock_guard lock(v_collector__conductor.v_mutex);
		if (--v_collector__full <= 0) v_collector__threshold = v_options.v_collector__threshold;
	}
}

template<typename T_type>
void t_engine<T_type>::f_finalize()
{
	f_epoch_region<T_type>([this]
	{
		std::unique_lock lock(v_finalizer__conductor.v_mutex);
		v_finalizer__conductor.f_wake();
		v_finalizer__conductor.f_wait(lock);
		v_finalizer__conductor.f_wake();
		v_finalizer__conductor.f_wait(lock);
	});
}

template<typename T_type>
template<typename T_thread, typename T_initialize, typename T_main>
void t_engine<T_type>::f_start(T_thread* a_thread, T_initialize a_initialize, T_main a_main)
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
			try {
				main();
			} catch (...) {
			}
			v_object__heap.f_return();
			f_epoch_region<T_type>([&, this]
			{
				std::lock_guard lock(v_thread__mutex);
				internal->v_background = false;
				a_thread->v_internal = nullptr;
			});
			t_slot<T_type>::t_decrements::f_push(a_thread);
			internal->f_epoch_done();
			std::lock_guard lock(v_thread__mutex);
			++internal->v_done;
			v_thread__condition.notify_all();
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
template<typename T_thread>
void t_engine<T_type>::f_join(T_thread* a_thread)
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
inline t_engine<T_type>* f_engine()
{
	return t_engine<T_type>::v_instance;
}

}

#endif
