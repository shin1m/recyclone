#include "type.h"

int main(int argc, char* argv[])
{
	t_engine<t_type>::t_options options;
	if (argc > 1) std::sscanf(argv[1], "%zu", &options.v_collector__threshold);
	options.v_verbose = true;
	t_engine<t_type> engine(options);
	engine.f_join_foregrounds();
	assert(engine.f_exiting());
	return 0;
}
