#include "thread.h"
#include "pair.h"

size_t v_finalized;
t_root<t_slot_of<t_pair>> v_resurrected;

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
		if (a_p->f_type() != &t_type_of<t_pair>::v_instance) return;
		auto s = f_string(a_p);
		f_epoch_region<t_type>([&]
		{
			std::printf("%s -> ", s.c_str());
		});
		auto p = static_cast<t_pair*>(a_p);
		if (p->v_head && p->v_head->f_type() == &t_type_of<t_symbol>::v_instance && static_cast<t_symbol*>(p->v_head)->v_name == "resurrected"sv) {
			++v_finalized;
			f_epoch_region<t_type>([]
			{
				std::printf("finalized\n");
			});
		} else {
			p->v_head = f_new<t_symbol>("resurrected"sv);
			v_resurrected = p;
			p->f_finalizee__(true);
			f_epoch_region<t_type>([]
			{
				std::printf("resurrected\n");
			});
		}
	});
	f_padding([]
	{
		f_epoch_point<t_type>();
		f_new<t_pair>(f_new<t_symbol>("foo"sv))->f_finalizee__(true);
	});
	engine.f_collect();
	engine.f_finalize();
	f_padding([]
	{
		f_epoch_point<t_type>();
		auto s = f_string(v_resurrected);
		f_epoch_region<t_type>([&]
		{
			std::printf("resurrected: %s\n", s.c_str());
		});
#ifndef NDEBUG
		assert(static_cast<t_object<t_type>*>(v_resurrected));
#endif
		v_resurrected = nullptr;
	});
	engine.f_collect();
	engine.f_finalize();
	f_epoch_region<t_type>([]
	{
		std::printf("finalized: %zu\n", v_finalized);
	});
#ifndef NDEBUG
	assert(v_finalized == 1);
#endif
	return engine.f_exit(0);
}
