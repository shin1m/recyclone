#ifndef RECYCLONE__OBJECT_H
#define RECYCLONE__OBJECT_H

#include "heap.h"
#include "slot.h"
#include <cassert>

namespace recyclone
{

template<typename> class t_extension;

template<typename T_type>
using t_scan = void(*)(t_object<T_type>*);

enum t_color : char
{
	e_color__BLACK,
	e_color__PURPLE,
	e_color__GRAY,
	e_color__WHITING,
	e_color__WHITE,
	e_color__ORANGE,
	e_color__RED
};

template<typename T_type>
class t_object
{
	static_assert(std::is_trivially_default_constructible_v<t_slot<T_type>>);

	friend T_type;
	template<typename> friend class t_heap;
	friend class t_slot<T_type>;
	friend class t_thread<T_type>;
	friend class t_engine<T_type>;
	friend class t_weak_pointer<T_type>;

	//! Roots for candidate cycles.
	static inline RECYCLONE__THREAD struct
	{
		t_object* v_next;
		t_object* v_previous;
	} v_roots;
	static inline RECYCLONE__THREAD t_object* v_scan_stack;
	static inline RECYCLONE__THREAD t_object* v_cycle;

	RECYCLONE__FORCE_INLINE static void f_append(t_object* a_p)
	{
		a_p->v_next = reinterpret_cast<t_object*>(&v_roots);
		a_p->v_previous = v_roots.v_previous;
		a_p->v_previous->v_next = v_roots.v_previous = a_p;
	}
	static void f_push(t_object* a_p)
	{
		a_p->v_scan = v_scan_stack;
		v_scan_stack = a_p;
	}
	template<void (t_object::*A_push)()>
	static void f_push(t_object* a_p)
	{
		if (a_p) a_p->template f_push<A_push>();
	}

	t_object* v_next;
	union
	{
		t_object* v_previous;
		t_object* v_chunk_next;
		t_object* v_next_cycle;
	};
	t_object* v_scan;
	t_color v_color;
	bool v_finalizee;
	char v_rank;
	size_t v_count;
	union
	{
		size_t v_cyclic;
		size_t v_chunk_size;
	};
	T_type* v_type;
	t_extension<T_type>* v_extension;
	T_type* v__type;

	template<void (t_object::*A_push)()>
	void f_push()
	{
		(this->*A_push)();
	}
	template<void (t_object::*A_push)()>
	void f_step()
	{
		v_type->f_scan(this, f_push<A_push>);
		v_type->template f_push<A_push>();
		if (auto p = std::atomic_ref(v_extension).load(std::memory_order_consume)) p->f_scan(f_push<A_push>);
	}
	template<void (t_object::*A_step)()>
	void f_loop()
	{
		auto p = this;
		while (true) {
			(p->*A_step)();
			p = v_scan_stack;
			if (!p) break;
			v_scan_stack = p->v_scan;
		}
	}
	RECYCLONE__FORCE_INLINE void f_increment()
	{
		++v_count;
		v_color = e_color__BLACK;
	}
	bool f_queue_finalize();
	void f_decrement_push()
	{
		assert(v_count > 0);
		if (--v_count > 0) {
			v_color = e_color__PURPLE;
			if (!v_next) f_append(this);
		} else if (!v_finalizee || !f_queue_finalize()) {
			f_push(this);
		}
	}
	void f_decrement_step()
	{
		if (auto p = v_extension) {
			p->f_scan(f_push<&t_object::f_decrement_push>);
			v_extension = nullptr;
			delete p;
		}
		v_type->f_finalize(this, f_push<&t_object::f_decrement_push>);
		v_type->f_decrement_push();
		v_type = nullptr;
		if (v_next) {
			if (reinterpret_cast<intptr_t>(v_previous) & 1) return;
			v_next->v_previous = v_previous;
			v_previous->v_next = v_next;
		}
		f_engine<T_type>()->f_free_as_release(this);
	}
	void f_decrement()
	{
		assert(v_count > 0);
		if (--v_count > 0) {
			v_color = e_color__PURPLE;
			if (!v_next) f_append(this);
		} else if (!v_finalizee || !f_queue_finalize()) {
			f_loop<&t_object::f_decrement_step>();
		}
	}
	void f_mark_gray_push()
	{
		if (v_color != e_color__GRAY) {
			v_color = e_color__GRAY;
			v_cyclic = v_count;
			f_push(this);
		}
		--v_cyclic;
	}
	void f_mark_gray()
	{
		v_color = e_color__GRAY;
		v_cyclic = v_count;
		f_loop<&t_object::f_step<&t_object::f_mark_gray_push>>();
	}
	void f_scan_black_push()
	{
		if (v_color == e_color__BLACK) return;
		v_color = e_color__BLACK;
		f_push(this);
	}
	void f_scan_gray_scan_black_push()
	{
		if (v_color == e_color__BLACK) return;
		if (v_color != e_color__WHITING) f_push(this);
		v_color = e_color__BLACK;
	}
	void f_scan_gray_push()
	{
		if (v_color != e_color__GRAY) return;
		v_color = v_cyclic > 0 ? e_color__BLACK : e_color__WHITING;
		f_push(this);
	}
	void f_scan_gray_step()
	{
		if (v_color == e_color__BLACK) {
			f_step<&t_object::f_scan_gray_scan_black_push>();
		} else {
			v_color = e_color__WHITE;
			f_step<&t_object::f_scan_gray_push>();
		}
	}
	void f_scan_gray()
	{
		if (v_color != e_color__GRAY) return;
		if (v_cyclic > 0) {
			v_color = e_color__BLACK;
			f_loop<&t_object::f_step<&t_object::f_scan_black_push>>();
		} else {
			f_loop<&t_object::f_scan_gray_step>();
		}
	}
	void f_collect_white_push()
	{
		if (v_color != e_color__WHITE) return;
		v_color = e_color__RED;
		v_cyclic = v_count;
		v_next = v_cycle->v_next;
		v_cycle->v_next = this;
		v_previous = reinterpret_cast<t_object*>(1);
		f_push(this);
	}
	void f_collect_white()
	{
		v_color = e_color__RED;
		v_cyclic = v_count;
		v_cycle = v_next = this;
		v_previous = reinterpret_cast<t_object*>(1);
		f_loop<&t_object::f_step<&t_object::f_collect_white_push>>();
	}
	void f_scan_red()
	{
		if (v_color == e_color__RED && v_cyclic > 0) --v_cyclic;
	}
	void f_cyclic_decrement_push()
	{
		if (v_color == e_color__RED) return;
		if (v_color == e_color__ORANGE) {
			--v_count;
			--v_cyclic;
		} else {
			f_decrement();
		}
	}
	void f_cyclic_decrement()
	{
		if (auto p = v_extension) {
			p->f_scan(f_push<&t_object::f_cyclic_decrement_push>);
			v_extension = nullptr;
			delete p;
		}
		v_type->f_finalize(this, f_push<&t_object::f_cyclic_decrement_push>);
		v_type->f_cyclic_decrement_push();
		v_type = nullptr;
	}
	void f_own()
	{
		t_slot<T_type>::t_increments::f_push(this);
	}

protected:
	t_object(size_t a_rank) : v_count(1), v_rank(a_rank)
	{
	}

public:
	using t_type = T_type;

	/*!
	  \sa t_engine::f_allocate clears v_next.
	  But when placement new follows it, it is eliminated by lifetime dse.
	  The initialization here is a workaround for this case.
	 */
	t_object() : v_next(nullptr)
	{
	}
	/*!
	  Finalizes the object construction.
	  \param a_type The type of the object.
	 */
	RECYCLONE__ALWAYS_INLINE void f_be(T_type* a_type)
	{
		a_type->f_own();
		std::atomic_signal_fence(std::memory_order_release);
		v_type = v__type = a_type;
		t_slot<T_type>::t_decrements::f_push(this);
	}
	//! Sets whether the finalizer should finalize the object.
	void f_finalizee__(bool a_value)
	{
		v_finalizee = a_value;
	}
	T_type* f_type() const
	{
		return v__type;
	}
	/*!
	  Lazily populates the extension part of the object.
	  \return The extension part.
	 */
	t_extension<T_type>* f_extension();
};

template<typename T_type>
bool t_object<T_type>::f_queue_finalize()
{
	auto engine = f_engine<T_type>();
	if (engine->v_finalizer__conductor.v_quitting) return false;
	f_increment();
	std::lock_guard lock(engine->v_finalizer__mutex);
	engine->v_finalizer__queue.push_back(this);
	engine->v_finalizer__conductor.f_wake();
	return true;
}

template<typename T_type>
t_extension<T_type>* t_object<T_type>::f_extension()
{
	auto p = std::atomic_ref(v_extension).load(std::memory_order_consume);
	if (p) return p;
	t_extension<T_type>* q = nullptr;
	p = new t_extension<T_type>;
	if (std::atomic_ref(v_extension).compare_exchange_strong(q, p, std::memory_order_consume)) return p;
	delete p;
	return q;
}

}

#endif
