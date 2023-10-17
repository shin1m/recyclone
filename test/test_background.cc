#include "thread.h"

int main(int argc, char* argv[])
{
	t_engine<t_type>::t_options options;
	if (argc > 1) std::sscanf(argv[1], "%zu", &options.v_collector__threshold);
	options.v_verbose = true;
	t_engine_with_threads engine(options);
	std::exit(engine.f_run([&]
	{
		engine.f_start_thread([]
		{
			f_epoch_point<t_type>();
			f_epoch_region<t_type>([]
			{
				std::this_thread::sleep_for(std::chrono::seconds::max());
			});
		}, true);
		return 0;
	}));
}
