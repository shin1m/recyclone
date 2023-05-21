#include "pair.h"

template<typename T>
struct t_fix : private T
{
	explicit constexpr t_fix(auto&& a_x) noexcept : T(std::forward<decltype(a_x)>(a_x))
	{
	}
	constexpr decltype(auto) operator()(auto&&... a_xs) const
	{
		return T::operator()(*this, std::forward<decltype(a_xs)>(a_xs)...);
	}
};

template<typename T>
t_fix(T&&) -> t_fix<std::decay_t<T>>;

t_pair* f_hanoi(t_pair* RECYCLONE__SPILL a_tower, auto a_move)
{
	return t_fix([&](auto step, t_pair* RECYCLONE__SPILL a_height, t_pair* RECYCLONE__SPILL a_towers, size_t a_from, size_t a_via, size_t a_to) -> t_pair*
	{
		f_epoch_point<t_type>();
		return a_height
			? step(static_cast<t_pair*>(a_height->v_tail), a_move(
				step(static_cast<t_pair*>(a_height->v_tail), a_towers, a_from, a_to, a_via),
				a_from, a_to
			), a_via, a_from, a_to)
			: a_move(a_towers, a_from, a_to);
	})(static_cast<t_pair*>(a_tower->v_tail), f_new<t_pair>(a_tower, f_new<t_pair>(nullptr, f_new<t_pair>(nullptr))), 0, 1, 2);
}

t_object<t_type>* f_get(t_pair* RECYCLONE__SPILL a_xs, size_t a_i)
{
	return a_i > 0 ? f_get(static_cast<t_pair*>(a_xs->v_tail), a_i - 1) : static_cast<t_object<t_type>*>(a_xs->v_head);
}

t_pair* f_put(t_pair* RECYCLONE__SPILL a_xs, size_t a_i, t_object<t_type>* RECYCLONE__SPILL a_x)
{
	f_epoch_point<t_type>();
	return a_i > 0
		? f_new<t_pair>(a_xs->v_head, f_put(static_cast<t_pair*>(a_xs->v_tail), a_i - 1, a_x))
		: f_new<t_pair>(a_x, a_xs->v_tail);
}

int main(int argc, char* argv[])
{
	t_engine<t_type>::t_options options;
	if (argc > 1) std::sscanf(argv[1], "%zu", &options.v_collector__threshold);
	options.v_verbose = options.v_verify = true;
	t_engine<t_type> engine(options);
	return engine.f_exit([&]
	{
		f_epoch_point<t_type>();
		auto towers = f_hanoi(
			f_new<t_pair>(f_new<t_symbol>("a"sv),
			f_new<t_pair>(f_new<t_symbol>("b"sv),
			f_new<t_pair>(f_new<t_symbol>("c"sv),
			f_new<t_pair>(f_new<t_symbol>("d"sv),
			f_new<t_pair>(f_new<t_symbol>("e"sv)
		))))), [](auto RECYCLONE__SPILL a_towers, auto RECYCLONE__SPILL a_from, auto RECYCLONE__SPILL a_to)
		{
			f_epoch_point<t_type>();
			auto s = f_string(a_towers);
			f_epoch_region<t_type>([&]
			{
				std::printf("%s\n", s.c_str());
			});
			auto RECYCLONE__SPILL tower = static_cast<t_pair*>(f_get(a_towers, a_from));
			return f_put(f_put(a_towers, a_from, tower->v_tail), a_to,
				f_new<t_pair>(tower->v_head, f_get(a_towers, a_to))
			);
		});
		auto s = f_string(towers);
		f_epoch_region<t_type>([&]
		{
			std::printf("%s\n", s.c_str());
		});
		assert(s == "(() () (a b c d e))");
		return 0;
	}());
}
