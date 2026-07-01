#include "pair.h"

t_pair* f_build(size_t a_n, t_pair* RECYCLONE__SPILL a_leaf)
{
	f_epoch_point<t_type>();
	if (a_n <= 0) return a_leaf;
	--a_n;
	auto RECYCLONE__SPILL p = f_new<t_pair>();
	p->v_head = f_build(a_n, p);
	p->v_tail = f_build(a_n, nullptr);
	return p;
}

int main(int argc, char* argv[])
{
	t_engine<t_type>::t_options options;
	if (argc > 1) std::sscanf(argv[1], "%zu", &options.v_collector__threshold);
	options.v_verbose = true;
	t_engine<t_type> engine(options);
	return []() RECYCLONE__NOINLINE
	{
		for (size_t i = 0; i < 8; ++i) {
			f_build(16, nullptr);
			f_epoch_point<t_type>();
		}
		return 0;
	}();
}
