#include "thread.h"
#include "pair.h"
#include <functional>

int main(int argc, char* argv[])
{
	t_engine<t_type>::t_options options;
	if (argc > 1) std::sscanf(argv[1], "%zu", &options.v_collector__threshold);
	options.v_verbose = options.v_verify = true;
	t_engine_with_threads engine(options);
	return engine.f_exit([&]
	{
		auto monitor = f_new<t_symbol>("monitor"sv);
		auto& mutex = monitor->f_extension()->v_mutex;
		auto& condition = monitor->f_extension()->v_condition;
		std::function<void()> action = []
		{
			f_epoch_point<t_type>();
		};
		::t_thread* worker;
		{
			{
				t_epoch_region<t_type> region;
				mutex.lock();
			}
			std::unique_lock lock(mutex, std::adopt_lock);
			worker = engine.f_start_thread([&]
			{
				f_epoch_point<t_type>();
				{
					t_epoch_region<t_type> region;
					std::printf("start\n");
				}
				try {
					while (true) {
						{
							{
								t_epoch_region<t_type> region;
								mutex.lock();
							}
							std::unique_lock lock(mutex, std::adopt_lock);
							action = nullptr;
							condition.notify_one();
							while (!action) {
								{
									t_epoch_region<t_type> region;
									condition.wait(lock);
								}
								f_epoch_point<t_type>();
							}
						}
						action();
						f_epoch_point<t_type>();
					}
				} catch (std::nullptr_t) {}
				t_epoch_region<t_type> region;
				std::printf("exit\n");
			});
			while (action) {
				{
					t_epoch_region<t_type> region;
					condition.wait(lock);
				}
				f_epoch_point<t_type>();
			}
		}
		auto send = [&](auto x)
		{
			f_epoch_point<t_type>();
			{
				t_epoch_region<t_type> region;
				mutex.lock();
			}
			std::unique_lock lock(mutex, std::adopt_lock);
			action = x;
			condition.notify_one();
			while (action) {
				{
					t_epoch_region<t_type> region;
					condition.wait(lock);
				}
				f_epoch_point<t_type>();
			}
		};
		auto log = ""s;
		send([&]
		{
			f_epoch_point<t_type>();
			log += "Hello, ";
		});
		send([&]
		{
			f_epoch_point<t_type>();
			log += "World.";
		});
		{
			{
				t_epoch_region<t_type> region;
				mutex.lock();
			}
			std::unique_lock lock(mutex, std::adopt_lock);
			action = []
			{
				f_epoch_point<t_type>();
				throw nullptr;
			};
			condition.notify_one();
		}
		engine.f_join(worker);
		{
			t_epoch_region<t_type> region;
			std::printf("%s\n", log.c_str());
		}
		assert(log == "Hello, World.");
		return 0;
	}());
}
