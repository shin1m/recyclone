#ifndef TEST__PAIR_H
#define TEST__PAIR_H

#include "type.h"
#include <string>

struct t_pair : t__object
{
	t_slot_of<t_object<t_type>> v_head;
	t_slot_of<t_object<t_type>> v_tail;

	//! Called by f_new<t_pair>(...).
	t_pair(t_object<t_type>* RECYCLONE__SPILL a_head = nullptr, t_object<t_type>* RECYCLONE__SPILL a_tail = nullptr) : v_head(a_head), v_tail(a_tail)
	{
		f_epoch_point<t_type>();
	}
	//! Called by t_type_of<t_pair>::f_finalize(...).
	/*~t_pair()
	{
	}*/
	//! Called by t_type_of<t_pair>::f_scan(...).
	void f_scan(t_scan<t_type> a_scan)
	{
		a_scan(v_head);
		a_scan(v_tail);
	}
};

struct t_symbol : t__object
{
	std::string v_name;

	//! Called by f_new<t_symbol>(...).
	t_symbol(std::string_view a_name) : v_name(a_name)
	{
		f_epoch_point<t_type>();
	}
};

inline std::string f_string(t_object<t_type>* RECYCLONE__SPILL a_value)
{
	f_epoch_point<t_type>();
	if (!a_value) return "()";
	if (a_value->f_type() == &t_type_of<t_pair>::v_instance)
		for (auto s = "("s;; s += ' ') {
			auto p = static_cast<t_pair*>(a_value);
			s += f_string(p->v_head);
			a_value = p->v_tail;
			if (!a_value) return s + ')';
			if (a_value->f_type() != &t_type_of<t_pair>::v_instance) return s + " . " + f_string(a_value) + ')';
			f_epoch_point<t_type>();
		}
	if (a_value->f_type() == &t_type_of<t_symbol>::v_instance) return static_cast<t_symbol*>(a_value)->v_name;
	throw std::runtime_error("unknown type");
}

#endif
