#include "pair.h"

int main(int argc, char* argv[])
{
	t_engine<t_type>::t_options options;
	if (argc > 1) std::sscanf(argv[1], "%zu", &options.v_collector__threshold);
	options.v_verbose = true;
	t_engine<t_type> engine(options);
	return []() RECYCLONE__NOINLINE
	{
		for (size_t i = 0; i < 8; ++i) {
			auto p = static_cast<t_symbol*>(f_engine<t_type>()->f_allocate(sizeof(t_object<t_type>) * (uintptr_t(1) << i) + sizeof(std::string)));
			new(p) t_symbol({});
			p->f_be(&t_type_of<t_symbol>::v_instance);
			f_epoch_point<t_type>();
		}
		return 0;
	}();
}
