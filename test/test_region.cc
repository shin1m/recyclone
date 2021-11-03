#include "pair.h"

int main(int argc, char* argv[])
{
	t_engine<t_type>::t_options options;
	if (argc > 1) std::sscanf(argv[1], "%zu", &options.v_collector__threshold);
	options.v_verbose = options.v_verify = true;
	t_engine<t_type> engine(options);
	return engine.f_exit([]
	{
		auto p = f_new<t_pair>(f_new<t_symbol>("outside"sv));
		auto s = f_string(p);
		f_epoch_region<t_type>([&]
		{
			std::printf("%s\n", s.c_str());
			f_epoch_noiger<t_type>([&]
			{
				p->v_tail = f_new<t_pair>(f_new<t_symbol>("inside"sv));
				s = f_string(p);
			});
			std::printf("%s\n", s.c_str());
		});
		return 0;
	}());
}
