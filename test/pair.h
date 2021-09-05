#ifndef TEST__PAIR_H
#define TEST__PAIR_H

#include "type.h"
#include <string>

using namespace std::literals;

struct t_pair : t_object<t_type>
{
	t_slot_of<t_object<t_type>> v_head;
	t_slot_of<t_object<t_type>> v_tail;

	//! Called by f_new<t_pair>(...).
	void f_construct(t_object<t_type>* a_head = nullptr, t_object<t_type>* a_tail = nullptr)
	{
		// Members have not been constructed yet at this point.
		new(&v_head) decltype(v_head)(a_head);
		new(&v_tail) decltype(v_tail)(a_tail);
	}
	//! Called by t_type_of<t_pair>::f_scan(...).
	void f_scan(t_scan<t_type> a_scan)
	{
		a_scan(v_head);
		a_scan(v_tail);
	}
	//! Called by t_type_of<t_pair>::f_finalize(...).
	void f_destruct()
	{
	}
};

struct t_symbol : t_object<t_type>
{
	std::string v_name;

	//! Called by f_new<t_symbol>(...).
	void f_construct(std::string_view a_name)
	{
		// Members have not been constructed yet at this point.
		new(&v_name) decltype(v_name)(a_name);
	}
	//! Called by t_type_of<t_symbol>::f_scan(...).
	void f_scan(t_scan<t_type>)
	{
	}
	template<typename T> using just = T;
	//! Called by t_type_of<t_symbol>::f_finalize(...).
	void f_destruct()
	{
		// Members must be destructed here.
		v_name.~just<decltype(v_name)>();
	}
};

inline std::string f_string(t_object<t_type>* a_value)
{
	if (!a_value) return "()";
	if (a_value->f_type() == &t_type_of<t_pair>::v_instance)
		for (auto s = "("s;; s += ' ') {
			auto p = static_cast<t_pair*>(a_value);
			s += f_string(p->v_head);
			a_value = p->v_tail;
			if (!a_value) return s + ')';
			if (a_value->f_type() != &t_type_of<t_pair>::v_instance) return s + " . " + f_string(a_value) + ')';
		}
	if (a_value->f_type() == &t_type_of<t_symbol>::v_instance) return static_cast<t_symbol*>(a_value)->v_name;
	throw std::runtime_error("unknown type");
}

#endif
