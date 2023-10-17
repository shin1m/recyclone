#include "pair.h"

int main(int argc, char* argv[])
{
	t_engine<t_type>::t_options options;
	if (argc > 1) std::sscanf(argv[1], "%zu", &options.v_collector__threshold);
	options.v_verbose = true;
	t_engine<t_type> engine(options);
	return engine.f_run([]
	{
		auto RECYCLONE__SPILL p = f_new<t_pair>();
		auto RECYCLONE__SPILL q = f_new<t_pair>(p);
		p->v_tail = q;
		return 0;
	});
}
