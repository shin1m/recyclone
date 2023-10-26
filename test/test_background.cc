#include "thread.h"

int main(int argc, char* argv[])
{
	t_engine<t_type>::t_options options;
	if (argc > 1) std::sscanf(argv[1], "%zu", &options.v_collector__threshold);
	options.v_verbose = true;
	t_engine_with_threads engine(options);
	[&]() RECYCLONE__NOINLINE
	{
		engine.f_start_thread([]
		{
			f_epoch_point<t_type>();
			f_epoch_region<t_type>([]
			{
				std::this_thread::sleep_for(std::chrono::seconds::max());
			});
		}, true);
		engine.f_join_foregrounds();
		std::exit(0);
	}();
}
