#include "thread.h"
#include "pair.h"

t_root<t_slot_of<t_symbol>> v_resurrected;

int main(int argc, char* argv[])
{
	t_engine<t_type>::t_options options;
	if (argc > 1) std::sscanf(argv[1], "%zu", &options.v_collector__threshold);
#ifdef NDEBUG
	options.v_verbose = true;
#else
	options.v_verbose = options.v_verify = true;
#endif
	t_engine_with_finalizer engine(options, [](auto RECYCLONE__SPILL a_p)
	{
		f_epoch_point<t_type>();
		if (a_p->f_type() != &t_type_of<t_symbol>::v_instance) return;
		auto p = static_cast<t_symbol*>(a_p);
		if (static_cast<t_symbol*>(p)->v_name == "resurrected"sv) return;
		p->v_name = "resurrected"sv;
		v_resurrected = p;
		p->f_finalizee__(true);
	});
	std::unique_ptr<t_weak_pointer<t_type>> w;
	f_padding([&]
	{
		f_epoch_point<t_type>();
		auto RECYCLONE__SPILL x = f_new<t_symbol>("foo"sv);
		w = std::make_unique<t_weak_pointer<t_type>>(x, false);
		assert(w->f_target() == x);
	});
	engine.f_collect();
#ifndef NDEBUG
	assert(w->f_target() == nullptr);
#endif
	f_padding([&]
	{
		f_epoch_point<t_type>();
		auto RECYCLONE__SPILL x = f_new<t_symbol>("bar"sv);
		x->f_finalizee__(true);
		w = std::make_unique<t_weak_pointer<t_type>>(x, true);
		assert(w->f_target() == x);
	});
	engine.f_collect();
	engine.f_finalize();
	f_padding([&]
	{
		f_epoch_point<t_type>();
		assert(w->f_target() != nullptr);
		v_resurrected = nullptr;
	});
	engine.f_collect();
	engine.f_finalize();
	engine.f_collect();
#ifndef NDEBUG
	assert(w->f_target() == nullptr);
#endif
	f_padding([&]
	{
		f_epoch_point<t_type>();
		auto RECYCLONE__SPILL x = f_new<t_symbol>("foo"sv);
		w = std::make_unique<t_weak_pointer<t_type>>(x, true);
		auto RECYCLONE__SPILL y = f_new<t_symbol>("bar"sv);
		w->f_target__(y);
		assert(w->f_target() == y);
	});
	return engine.f_exit(0);
}
