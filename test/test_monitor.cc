#include "thread.h"
#include "extension.h"
#include "extension.cc"
#include <functional>

int main(int argc, char* argv[])
{
	t_engine<t_type>::t_options options;
	if (argc > 1) std::sscanf(argv[1], "%zu", &options.v_collector__threshold);
	options.v_verbose = true;
	t_engine_with_threads engine(options);
	return [&]() RECYCLONE__NOINLINE
	{
		auto RECYCLONE__SPILL monitor = f_new<t_object_with_extension>();
		auto& mutex = monitor->f_extension()->v_mutex;
		auto& condition = monitor->f_extension()->v_condition;
		std::function<void()> action = []
		{
			f_epoch_point<t_type>();
		};
		::t_thread* RECYCLONE__SPILL worker;
		{
			f_epoch_region<t_type>([&]
			{
				mutex.lock();
			});
			std::unique_lock lock(mutex, std::adopt_lock);
			worker = engine.f_start_thread([&]
			{
				f_epoch_point<t_type>();
				f_epoch_region<t_type>([]
				{
					std::printf("start\n");
				});
				try {
					while (true) {
						{
							f_epoch_region<t_type>([&]
							{
								mutex.lock();
							});
							std::unique_lock lock(mutex, std::adopt_lock);
							action = nullptr;
							condition.notify_one();
							while (!action) {
								f_epoch_region<t_type>([&]
								{
									condition.wait(lock);
								});
								f_epoch_point<t_type>();
							}
						}
						action();
						f_epoch_point<t_type>();
					}
#ifdef _MSC_VER
				} catch (int) {}
#else
				} catch (std::nullptr_t) {}
#endif
				f_epoch_region<t_type>([]
				{
					std::printf("exit\n");
				});
			});
			while (action) {
				f_epoch_region<t_type>([&]
				{
					condition.wait(lock);
				});
				f_epoch_point<t_type>();
			}
		}
		auto send = [&](auto x)
		{
			f_epoch_point<t_type>();
			f_epoch_region<t_type>([&]
			{
				mutex.lock();
			});
			std::unique_lock lock(mutex, std::adopt_lock);
			action = x;
			condition.notify_one();
			while (action) {
				f_epoch_region<t_type>([&]
				{
					condition.wait(lock);
				});
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
			f_epoch_region<t_type>([&]
			{
				mutex.lock();
			});
			std::unique_lock lock(mutex, std::adopt_lock);
			action = []
			{
				f_epoch_point<t_type>();
#ifdef _MSC_VER
				throw 0;
#else
				throw nullptr;
#endif
			};
			condition.notify_one();
		}
		engine.f_join(worker);
		f_epoch_region<t_type>([&]
		{
			std::printf("%s\n", log.c_str());
		});
		assert(log == "Hello, World.");
		engine.f_join_foregrounds();
		return 0;
	}();
}
