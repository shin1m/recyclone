#include "thread.h"
#include "pair.h"

t_root<t_slot_of<t_symbol>> v_resurrected;

int main(int argc, char* argv[])
{
	t_engine<t_type>::t_options options;
	if (argc > 1) std::sscanf(argv[1], "%zu", &options.v_collector__threshold);
	options.v_verbose = true;
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
#ifdef NDEBUG
	std::exit(engine.f_run([&]
#else
	return engine.f_run([&]
#endif
	{
		auto pair = [](t_object<t_type>* x, t_object<t_type>* y)
		{
			return std::make_pair(x, y);
		};
		std::unique_ptr<t_weak_pointer<t_type>> w;
		f_with_scratch([&]
		{
			f_epoch_point<t_type>();
			auto RECYCLONE__SPILL x = f_new<t_symbol>("foo"sv);
			w = std::make_unique<t_weak_pointer<t_type>>(x, false);
			assert(w->f_get() == pair(x, nullptr));
		});
		engine.f_collect();
#ifndef NDEBUG
		assert(w->f_get() == pair(nullptr, nullptr));
#endif
		f_with_scratch([&]
		{
			f_epoch_point<t_type>();
			auto RECYCLONE__SPILL x = f_new<t_symbol>("foo"sv);
			auto RECYCLONE__SPILL y = f_new<t_symbol>("bar"sv);
			w = std::make_unique<t_weak_pointer<t_type>>(x, y);
			assert(w->f_get() == pair(x, y));
			auto RECYCLONE__SPILL z = f_new<t_symbol>("zot"sv);
			w->f_dependent__(z);
			assert(w->f_get() == pair(x, z));
		});
		engine.f_collect();
#ifndef NDEBUG
		assert(w->f_get() == pair(nullptr, nullptr));
#endif
		f_with_scratch([&]
		{
			f_epoch_point<t_type>();
			auto RECYCLONE__SPILL x = f_new<t_symbol>("bar"sv);
			x->f_finalizee__(true);
			w = std::make_unique<t_weak_pointer<t_type>>(x, true);
			assert(w->f_get() == pair(x, nullptr));
		});
		engine.f_collect();
		engine.f_finalize();
		f_with_scratch([&]
		{
			f_epoch_point<t_type>();
			assert(w->f_get().first != nullptr);
			v_resurrected = nullptr;
		});
		engine.f_collect();
		engine.f_finalize();
		engine.f_collect();
#ifndef NDEBUG
		assert(w->f_get() == pair(nullptr, nullptr));
#endif
		f_with_scratch([&]
		{
			f_epoch_point<t_type>();
			auto RECYCLONE__SPILL x = f_new<t_symbol>("foo"sv);
			w = std::make_unique<t_weak_pointer<t_type>>(x, true);
			auto RECYCLONE__SPILL y = f_new<t_symbol>("bar"sv);
			w->f_target__(y);
			assert(w->f_get() == pair(y, nullptr));
		});
		return 0;
#ifdef NDEBUG
	}));
#else
	});
#endif
}
