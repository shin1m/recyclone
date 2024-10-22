#include "extension.h"

void t_weak_pointer::f_attach(t_root<t_slot_of<t_object_with_extension>>& a_target)
{
	v_target = a_target;
	if (!v_target) return;
	auto extension = v_target->f_extension();
	std::lock_guard lock(extension->v_weak_pointers__mutex);
	if (!extension->v_weak_pointers__cycle) extension->v_weak_pointers__cycle.f_raw().store(a_target.f_raw().exchange(nullptr, std::memory_order_relaxed), std::memory_order_relaxed);
	this->v_previous = extension->v_weak_pointers.v_previous;
	this->v_next = static_cast<t_weak_pointer*>(&extension->v_weak_pointers);
	this->v_previous->v_next = this->v_next->v_previous = this;
}

t_object<t_type>* t_weak_pointer::f_detach()
{
	if (!v_target) return nullptr;
	auto extension = v_target->v_extension;
	std::lock_guard lock(extension->v_weak_pointers__mutex);
	this->v_previous->v_next = this->v_next;
	this->v_next->v_previous = this->v_previous;
	return extension->v_weak_pointers.v_next == &extension->v_weak_pointers ? extension->v_weak_pointers__cycle.f_raw().exchange(nullptr, std::memory_order_relaxed) : nullptr;
}

t_weak_pointer::t_weak_pointer(t_object_with_extension* a_target, bool a_final) : v_final(a_final)
{
	t_root<t_slot_of<t_object_with_extension>> p = a_target;
	f_engine<t_type>()->f_revive([&](auto)
	{
		f_attach(p);
	});
}

t_weak_pointer::t_weak_pointer(t_object_with_extension* a_target, t_object<t_type>* a_dependent)
{
	if (!a_target) a_dependent = nullptr;
	if (a_dependent) t_slot<t_type>::f_increment(a_dependent);
	t_root<t_slot_of<t_object_with_extension>> p = a_target;
	f_engine<t_type>()->f_revive([&](auto)
	{
		f_attach(p);
		v_dependent = a_dependent;
	});
}

t_weak_pointer::~t_weak_pointer()
{
	if (auto p = f_engine<t_type>()->f_revive([&](auto a_revive)
	{
		auto p = f_detach();
		if (p) a_revive(p);
		return p;
	})) t_slot<t_type>::f_decrement(p);
	if (v_dependent) t_slot<t_type>::f_decrement(v_dependent);
}

std::pair<t_object<t_type>*, t_object<t_type>*> t_weak_pointer::f_get() const
{
	return f_engine<t_type>()->f_revive([&](auto a_revive)
	{
		if (v_target) a_revive(v_target);
		return std::make_pair(v_target, v_dependent);
	});
}

void t_weak_pointer::f_target__(t_object_with_extension* a_p)
{
	t_root<t_slot<t_type>> dependent;
	t_root<t_slot_of<t_object_with_extension>> p = a_p;
	if (auto q = f_engine<t_type>()->f_revive([&](auto a_revive)
	{
		if (!a_p) v_dependent = dependent.f_raw().exchange(v_dependent, std::memory_order_relaxed);
		auto q = f_detach();
		if (q) a_revive(q);
		f_attach(p);
		return q;
	})) t_slot<t_type>::f_decrement(q);
}

void t_weak_pointer::f_dependent__(t_object<t_type>* a_p)
{
	if (v_final) throw std::runtime_error("cannot have a dependent.");
	t_root<t_slot<t_type>> dependent = a_p;
	f_engine<t_type>()->f_revive([&](auto)
	{
		if (v_target) v_dependent = dependent.f_raw().exchange(v_dependent, std::memory_order_relaxed);
	});
}

t_extension::~t_extension()
{
	for (auto p = v_weak_pointers.v_next; p != &v_weak_pointers; p = p->v_next) {
		p->v_target = nullptr;
		p->v_dependent = nullptr;
	}
}

void t_extension::f_detach()
{
	for (auto p = v_weak_pointers.v_next; p != &v_weak_pointers; p = p->v_next) {
		if (p->v_final) continue;
		p->v_target = nullptr;
		p->v_dependent = nullptr;
		p->v_previous->v_next = p->v_next;
		p->v_next->v_previous = p->v_previous;
	}
}

void t_extension::f_scan(t_scan<t_type> a_scan)
{
	std::lock_guard lock(v_weak_pointers__mutex);
	a_scan(v_weak_pointers__cycle);
	for (auto p = v_weak_pointers.v_next; p != &v_weak_pointers; p = p->v_next) a_scan(p->v_dependent);
}

t_extension* t_object_with_extension::f_extension()
{
	auto p = std::atomic_ref(v_extension).load(std::memory_order_consume);
	if (p) return p;
	t_extension* q = nullptr;
	p = new t_extension;
	if (std::atomic_ref(v_extension).compare_exchange_strong(q, p, std::memory_order_consume)) return p;
	delete p;
	return q;
}
