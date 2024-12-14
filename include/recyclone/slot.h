#ifndef RECYCLONE__SLOT_H
#define RECYCLONE__SLOT_H

#include "define.h"
#include <atomic>

#ifndef __cpp_lib_atomic_ref
// TODO Workaround for limited usage as temporary objects.
namespace std
{
template<typename T>
inline atomic<T>& atomic_ref(T& a_x)
{
	return reinterpret_cast<atomic<T>&>(a_x);
}
template<typename T>
inline const atomic<T>& atomic_ref(const T& a_x)
{
	return reinterpret_cast<const atomic<T>&>(a_x);
}
}
#endif

namespace recyclone
{

template<typename> class t_object;
template<typename> class t_thread;
template<typename> class t_engine;
template<typename T_type>
t_engine<T_type>* f_engine();

template<typename T_type>
class t_slot
{
	friend class t_object<T_type>;
	friend class t_thread<T_type>;
	friend class t_engine<T_type>;

	template<size_t A_SIZE>
	struct t_queue
	{
		static inline RECYCLONE__THREAD t_queue* v_instance;

		RECYCLONE__ALWAYS_INLINE static void f_push(t_object<T_type>* a_object)
		{
			v_instance->f__push(a_object);
		}

		t_object<T_type>* volatile* v_head = v_objects;
		t_object<T_type>* volatile* v_next = v_objects + A_SIZE / 8;
		t_object<T_type>* volatile v_objects[A_SIZE];
		std::atomic<t_object<T_type>* volatile*> v_epoch;
		t_object<T_type>* volatile* v_tail = v_objects + A_SIZE - 1;

		void f_next() noexcept;
		RECYCLONE__ALWAYS_INLINE void f__push(t_object<T_type>* a_object)
		{
			*v_head = a_object;
			if (v_head == v_next)
				f_next();
			else
				[[likely]] ++v_head;
		}
		void f__flush(t_object<T_type>* volatile* a_epoch, auto a_do)
		{
			auto end = v_objects + A_SIZE - 1;
			if (a_epoch > v_objects)
				--a_epoch;
			else
				a_epoch = end;
			while (v_tail != a_epoch) {
				auto next = a_epoch;
				if (v_tail < end) {
					if (next < v_tail) next = end;
					++v_tail;
				} else {
					v_tail = v_objects;
				}
				while (true) {
					a_do(*v_tail);
					if (v_tail == next) break;
					++v_tail;
				}
			}
		}
	};
#ifdef NDEBUG
	struct t_increments : t_queue<16384>
#else
	struct t_increments : t_queue<128>
#endif
	{
		void f_flush()
		{
			this->f__flush(this->v_epoch.load(std::memory_order_acquire), [](auto x)
			{
				x->f_increment();
			});
		}
	};
#ifdef NDEBUG
	struct t_decrements : t_queue<32768>
#else
	struct t_decrements : t_queue<256>
#endif
	{
		t_object<T_type>* volatile* v_last = this->v_objects;

		void f_flush()
		{
			this->f__flush(v_last, [](auto x)
			{
				x->f_decrement();
			});
			v_last = this->v_epoch.load(std::memory_order_acquire);
		}
	};

protected:
	t_object<T_type>* v_p;

public:
	static void f_increment(t_object<T_type>* a_p)
	{
		t_increments::f_push(a_p);
	}
	static void f_decrement(t_object<T_type>* a_p)
	{
		t_decrements::f_push(a_p);
	}

	t_slot() = default;
	t_slot(t_object<T_type>* a_p) : v_p(a_p)
	{
		if (a_p) t_increments::f_push(a_p);
	}
	t_slot(const t_slot& a_value) : t_slot(static_cast<t_object<T_type>*>(a_value))
	{
	}
	RECYCLONE__ALWAYS_INLINE t_slot& operator=(t_object<T_type>* a_p)
	{
		if (a_p) t_increments::f_push(a_p);
		if (auto p = std::atomic_ref(v_p).exchange(a_p, std::memory_order_relaxed)) t_decrements::f_push(p);
		return *this;
	}
	RECYCLONE__ALWAYS_INLINE t_slot& operator=(const t_slot& a_value)
	{
		return *this = static_cast<t_object<T_type>*>(a_value);
	}
	void f_destruct()
	{
		if (auto p = std::atomic_ref(v_p).load(std::memory_order_relaxed)) t_decrements::f_push(p);
	}
	operator bool() const
	{
		return std::atomic_ref(v_p).load(std::memory_order_relaxed);
	}
	operator t_object<T_type>*() const
	{
		return std::atomic_ref(v_p).load(std::memory_order_relaxed);
	}
	template<typename T>
	explicit operator T*() const
	{
		return static_cast<T*>(std::atomic_ref(v_p).load(std::memory_order_relaxed));
	}
	t_object<T_type>* operator->() const
	{
		return std::atomic_ref(v_p).load(std::memory_order_relaxed);
	}
#ifdef __cpp_lib_atomic_ref
	std::atomic_ref<t_object<T_type>*> f_raw()
#else
	auto& f_raw()
#endif
	{
		return std::atomic_ref(v_p);
	}
};

template<typename T_type>
template<size_t A_SIZE>
void t_slot<T_type>::t_queue<A_SIZE>::f_next() noexcept
{
	auto engine = f_engine<T_type>();
	engine->f_tick();
	if (v_head < v_objects + A_SIZE - 1) {
		++v_head;
		while (v_tail == v_head) engine->f_wait();
		auto tail = v_tail;
		v_next = std::min(tail < v_head ? v_objects + A_SIZE - 1 : tail - 1, v_head + A_SIZE / 8);
	} else {
		v_head = v_objects;
		while (v_tail == v_head) engine->f_wait();
		v_next = std::min(v_tail - 1, v_head + A_SIZE / 8);
	}
}

template<typename T, typename T_type = T::t_type>
struct t_slot_of : t_slot<T_type>
{
	using t_slot<T_type>::t_slot;
	RECYCLONE__ALWAYS_INLINE t_slot_of& operator=(const t_slot_of& a_value)
	{
		static_cast<t_slot<T_type>&>(*this) = a_value;
		return *this;
	}
	RECYCLONE__ALWAYS_INLINE t_slot_of& operator=(auto&& a_value)
	{
		static_cast<t_slot<T_type>&>(*this) = std::forward<decltype(a_value)>(a_value);
		return *this;
	}
	operator T*() const
	{
		return static_cast<T*>(std::atomic_ref(this->v_p).load(std::memory_order_relaxed));
	}
	T* operator->() const
	{
		return static_cast<T*>(std::atomic_ref(this->v_p).load(std::memory_order_relaxed));
	}
};

template<typename T>
struct t_root : T
{
	t_root() : T{}
	{
	}
	t_root(const t_root& a_value) : T(a_value)
	{
	}
	t_root(auto&& a_value) : T(std::forward<decltype(a_value)>(a_value))
	{
	}
	~t_root()
	{
		this->f_destruct();
	}
	t_root& operator=(const t_root& a_value)
	{
		static_cast<T&>(*this) = a_value;
		return *this;
	}
	t_root& operator=(auto&& a_value)
	{
		static_cast<T&>(*this) = std::forward<decltype(a_value)>(a_value);
		return *this;
	}
};

}

#endif
