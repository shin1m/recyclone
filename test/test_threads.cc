#include "thread.h"
#include "pair.h"

int main(int argc, char* argv[])
{
	t_engine<t_type>::t_options options;
	if (argc > 1) std::sscanf(argv[1], "%zu", &options.v_collector__threshold);
	options.v_verbose = options.v_verify = true;
	t_engine_with_threads engine(options);
	return engine.f_exit([&]() RECYCLONE__NOINLINE
	{
		f_epoch_point<t_type>();
		auto p = f_new<t_pair>();
		::t_thread* ts[10];
		for (size_t i = 0; i < 10; ++i) {
			ts[i] = engine.f_start_thread([p, i]
			{
				f_epoch_point<t_type>();
				f_epoch_region<t_type>([&]
				{
					std::printf("%zu\n", i);
				});
				for (size_t j = 0; j < 100; ++j) {
					p->v_head = f_new<t_pair>(f_new<t_symbol>(std::to_string(i)), p->v_head);
					f_epoch_point<t_type>();
				}
			});
			f_epoch_point<t_type>();
		}
		for (auto t : ts) {
			engine.f_join(t);
			f_epoch_point<t_type>();
		}
		auto s = f_string(p->v_head);
		f_epoch_region<t_type>([&]
		{
			std::printf("%s\n", s.c_str());
		});
		return 0;
	}());
}
