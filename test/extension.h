#ifndef TEST__EXTENSION_H
#define TEST__EXTENSION_H

#include "type.h"

class t_weak_pointer;
class t_extension;

class t_object_with_extension : public t_object<t_type>
{
	friend class t_weak_pointer;

	t_extension* v_extension = nullptr;

public:
	~t_object_with_extension();
	void f_scan(t_scan<t_type> a_scan);
	void f_prepare_for_finalizer();
	/*!
	  Lazily populates the extension part of the object.
	  \return The extension part.
	 */
	t_extension* f_extension();
};

struct t_weak_pointers
{
	t_weak_pointer* v_previous;
	t_weak_pointer* v_next;

	t_weak_pointers();
};

class t_weak_pointer : t_weak_pointers
{
	friend class t_weak_pointers;
	friend class t_extension;

	t_object_with_extension* v_target;
	t_object<t_type>* v_dependent = nullptr;

	void f_attach(t_root<t_slot_of<t_object_with_extension>>& a_target);
	t_object<t_type>* f_detach();

public:
	const bool v_final = false;

	t_weak_pointer(t_object_with_extension* a_target, bool a_final);
	t_weak_pointer(t_object_with_extension* a_target, t_object<t_type>* a_dependent);
	~t_weak_pointer();
	std::pair<t_object<t_type>*, t_object<t_type>*> f_get() const;
	void f_target__(t_object_with_extension* a_p);
	void f_dependent__(t_object<t_type>* a_p);
};

class t_extension
{
	friend class t_object_with_extension;
	friend class t_weak_pointer;

	t_weak_pointers v_weak_pointers;
	t_slot<t_type> v_weak_pointers__cycle{};
	std::mutex v_weak_pointers__mutex;

	~t_extension();
	void f_detach();
	void f_scan(t_scan<t_type> a_scan);

public:
	std::recursive_timed_mutex v_mutex;
	std::condition_variable_any v_condition;
};

inline t_weak_pointers::t_weak_pointers() : v_previous(static_cast<t_weak_pointer*>(this)), v_next(static_cast<t_weak_pointer*>(this))
{
}

inline t_object_with_extension::~t_object_with_extension()
{
	delete v_extension;
}

inline void t_object_with_extension::f_scan(t_scan<t_type> a_scan)
{
	if (auto p = std::atomic_ref(v_extension).load(std::memory_order_consume)) p->f_scan(a_scan);
}

inline void t_object_with_extension::f_prepare_for_finalizer()
{
	if (v_extension) v_extension->f_detach();
}

#endif
