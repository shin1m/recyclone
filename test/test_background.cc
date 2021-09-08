#include "thread.h"

int main(int argc, char* argv[])
{
	t_engine<t_type>::t_options options;
	if (argc > 1) std::sscanf(argv[1], "%zu", &options.v_collector__threshold);
	options.v_verbose = true;
	t_engine_with_threads engine(options);
	engine.f_start_thread([]
	{
		f_epoch_point<t_type>();
		t_epoch_region<t_type> region;
		std::this_thread::sleep_for(std::chrono::seconds::max());
	}, true);
	return engine.f_exit(0);
}
