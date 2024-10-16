#ifndef TEST__TYPE_H
#define TEST__TYPE_H

#include "define.h"

//! An example type descriptor that is not an object.
struct t_type
{
	//! Called by the collector to scan object members.
	void (*f_scan)(t_object<t_type>*, t_scan<t_type>);
	//! Called by the collector to finalize this.
	/*!
	  It is necessary to scan object members as in f_scan.
	  This is where to destruct native part.
	 */
	void (*f_finalize)(t_object<t_type>*, t_scan<t_type>);
	// Below are dummy implementations to skip scanning.
	template<void (t_object<t_type>::*A_push)()>
	void f_push()
	{
	}
	void f_decrement_push()
	{
	}
	void f_cyclic_decrement_push()
	{
	}
	void f_own()
	{
	}
	bool (*f_reviving)(t_object<t_type>*);
	void (*f_prepare_for_finalizer)(t_object<t_type>*);
};

struct t__object : t_object<t_type>
{
	//! Called by t_type_of<T>::f_scan(...).
	void f_scan(t_scan<t_type>)
	{
	}
	bool f_reviving() const
	{
		return false;
	}
	void f_prepare_for_finalizer()
	{
	}
};

template<typename T>
struct t_type_of : t_type
{
	static t_type_of v_instance;

	t_type_of()
	{
		f_scan = [](auto a_this, auto a_scan)
		{
			// Just delegates to a_this.
			static_cast<T*>(a_this)->f_scan(a_scan);
		};
		f_finalize = [](auto a_this, auto a_scan)
		{
			// Just delegates to a_this.
			auto p = static_cast<T*>(a_this);
			p->f_scan(a_scan);
			p->~T();
		};
		f_reviving = [](auto a_this)
		{
			// Just delegates to a_this.
			return static_cast<T*>(a_this)->f_reviving();
		};
		f_prepare_for_finalizer = [](auto a_this)
		{
			// Just delegates to a_this.
			static_cast<T*>(a_this)->f_prepare_for_finalizer();
		};
	}
};

template<typename T>
t_type_of<T> t_type_of<T>::v_instance;

template<typename T>
T* f_new(auto&&... a_xs)
{
	f_epoch_point<t_type>();
	auto p = static_cast<T*>(f_engine<t_type>()->f_allocate(sizeof(T)));
	if constexpr (sizeof...(a_xs) > 0)
		new(p) T(std::forward<decltype(a_xs)>(a_xs)...);
	else
		//! To avoid zero initialization.
		new(p) T;
	// Finishes object construction.
	p->f_be(&t_type_of<T>::v_instance);
	return p;
}

#endif
