#ifndef RECYCLONE__EXTENSION_H
#define RECYCLONE__EXTENSION_H

#include "object.h"
#include <condition_variable>

namespace recyclone
{

template<typename T_type>
struct t_weak_pointers
{
	t_weak_pointer<T_type>* v_previous;
	t_weak_pointer<T_type>* v_next;

	t_weak_pointers() : v_previous(static_cast<t_weak_pointer<T_type>*>(this)), v_next(static_cast<t_weak_pointer<T_type>*>(this))
	{
	}
};

template<typename T_type>
class t_weak_pointer : t_weak_pointers<T_type>
{
	friend struct t_weak_pointers<T_type>;
	friend class t_extension<T_type>;

	t_object<T_type>* v_target;
	t_object<T_type>* v_dependent = nullptr;

	auto f_reviving(auto a_do) const
	{
		f_engine<T_type>()->v_object__reviving__mutex.lock();
		auto p = v_target;
		if (p) {
			f_engine<T_type>()->v_object__reviving = true;
			t_thread<T_type>::v_current->f_revive();
		}
		auto x = a_do();
		f_engine<T_type>()->v_object__reviving__mutex.unlock();
		if (p) {
			t_slot<T_type>::t_increments::f_push(p);
			t_slot<T_type>::t_decrements::f_push(p);
		}
		return x;
	}
	void f_attach(t_root<t_slot<T_type>>& a_target);
	t_object<T_type>* f_detach();

public:
	const bool v_final = false;

	t_weak_pointer(t_object<T_type>* a_target, bool a_final);
	t_weak_pointer(t_object<T_type>* a_target, t_object<T_type>* a_dependent);
	~t_weak_pointer();
	std::pair<t_object<T_type>*, t_object<T_type>*> f_get() const;
	void f_target__(t_object<T_type>* a_p);
	void f_dependent__(t_object<T_type>* a_p);
};

template<typename T_type>
void t_weak_pointer<T_type>::f_attach(t_root<t_slot<T_type>>& a_target)
{
	v_target = a_target;
	if (!v_target) return;
	auto extension = v_target->f_extension();
	std::lock_guard lock(extension->v_weak_pointers__mutex);
	if (!extension->v_weak_pointers__cycle) std::atomic_ref(extension->v_weak_pointers__cycle.v_p).store(std::atomic_ref(a_target.v_p).exchange(nullptr, std::memory_order_relaxed), std::memory_order_relaxed);
	this->v_previous = extension->v_weak_pointers.v_previous;
	this->v_next = static_cast<t_weak_pointer<T_type>*>(&extension->v_weak_pointers);
	this->v_previous->v_next = this->v_next->v_previous = this;
}

template<typename T_type>
t_object<T_type>* t_weak_pointer<T_type>::f_detach()
{
	if (!v_target) return nullptr;
	auto extension = v_target->v_extension;
	std::lock_guard lock(extension->v_weak_pointers__mutex);
	this->v_previous->v_next = this->v_next;
	this->v_next->v_previous = this->v_previous;
	return extension->v_weak_pointers.v_next == &extension->v_weak_pointers ? std::atomic_ref(extension->v_weak_pointers__cycle.v_p).exchange(nullptr, std::memory_order_relaxed) : nullptr;
}

template<typename T_type>
t_weak_pointer<T_type>::t_weak_pointer(t_object<T_type>* a_target, bool a_final) : v_final(a_final)
{
	t_root<t_slot<T_type>> p = a_target;
	std::lock_guard lock(f_engine<T_type>()->v_object__reviving__mutex);
	f_attach(p);
}

template<typename T_type>
t_weak_pointer<T_type>::t_weak_pointer(t_object<T_type>* a_target, t_object<T_type>* a_dependent)
{
	if (!a_target) a_dependent = nullptr;
	if (a_dependent) t_slot<T_type>::t_increments::f_push(a_dependent);
	t_root<t_slot<T_type>> p = a_target;
	std::lock_guard lock(f_engine<T_type>()->v_object__reviving__mutex);
	f_attach(p);
	v_dependent = a_dependent;
}

template<typename T_type>
t_weak_pointer<T_type>::~t_weak_pointer()
{
	auto [p, q] = f_reviving([&]
	{
		return std::make_pair(f_detach(), v_dependent);
	});
	if (p) t_slot<T_type>::t_decrements::f_push(p);
	if (q) t_slot<T_type>::t_decrements::f_push(q);
}

template<typename T_type>
std::pair<t_object<T_type>*, t_object<T_type>*> t_weak_pointer<T_type>::f_get() const
{
	return f_reviving([&]
	{
		return std::make_pair(v_target, v_dependent);
	});
}

template<typename T_type>
void t_weak_pointer<T_type>::f_target__(t_object<T_type>* a_p)
{
	t_root<t_slot<T_type>> dependent;
	t_root<t_slot<T_type>> p = a_p;
	if (auto q = f_reviving([&]
	{
		if (!a_p) v_dependent = std::atomic_ref(dependent.v_p).exchange(v_dependent, std::memory_order_relaxed);
		auto r = f_detach();
		f_attach(p);
		return r;
	})) t_slot<T_type>::t_decrements::f_push(q);
}

template<typename T_type>
void t_weak_pointer<T_type>::f_dependent__(t_object<T_type>* a_p)
{
	if (v_final) throw std::runtime_error("cannot have a dependent.");
	t_root<t_slot<T_type>> dependent = a_p;
	f_reviving([&]
	{
		if (v_target) v_dependent = std::atomic_ref(dependent.v_p).exchange(v_dependent, std::memory_order_relaxed);
		return false;
	});
}

template<typename T_type>
class t_extension
{
	friend class t_object<T_type>;
	friend class t_engine<T_type>;
	friend class t_weak_pointer<T_type>;

	t_weak_pointers<T_type> v_weak_pointers;
	t_slot<T_type> v_weak_pointers__cycle{};
	std::mutex v_weak_pointers__mutex;

	~t_extension();
	void f_detach();
	void f_scan(t_scan<T_type> a_scan);

public:
	std::recursive_timed_mutex v_mutex;
	std::condition_variable_any v_condition;
};

template<typename T_type>
t_extension<T_type>::~t_extension()
{
	for (auto p = v_weak_pointers.v_next; p != &v_weak_pointers; p = p->v_next) p->v_target = p->v_dependent = nullptr;
}

template<typename T_type>
void t_extension<T_type>::f_detach()
{
	for (auto p = v_weak_pointers.v_next; p != &v_weak_pointers; p = p->v_next) {
		if (p->v_final) continue;
		p->v_target = p->v_dependent = nullptr;
		p->v_previous->v_next = p->v_next;
		p->v_next->v_previous = p->v_previous;
	}
}

template<typename T_type>
void t_extension<T_type>::f_scan(t_scan<T_type> a_scan)
{
	std::lock_guard lock(v_weak_pointers__mutex);
	a_scan(v_weak_pointers__cycle);
	for (auto p = v_weak_pointers.v_next; p != &v_weak_pointers; p = p->v_next) a_scan(p->v_dependent);
}

}

#endif
