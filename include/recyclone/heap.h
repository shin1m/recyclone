#ifndef RECYCLONE__HEAP_H
#define RECYCLONE__HEAP_H

#include "define.h"
#include <atomic>
#include <bit>
#include <map>
#include <mutex>
#include <system_error>
#ifdef __unix__
#include <sys/mman.h>
#endif
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace recyclone
{

template<typename T>
class t_heap
{
	static void* f_map(size_t a_n)
	{
#ifdef __unix__
		auto p = mmap(NULL, a_n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (p == MAP_FAILED) throw std::system_error(errno, std::system_category());
#endif
#ifdef _WIN32
		auto p = VirtualAlloc(NULL, a_n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (p == NULL) throw std::system_error(GetLastError(), std::system_category());
#endif
		return p;
	}
	static void f_unmap(void* a_p, size_t a_n)
	{
#ifdef __unix__
		munmap(a_p, a_n);
#endif
#ifdef _WIN32
		VirtualFree(a_p, 0, MEM_RELEASE);
#endif
	}

	template<size_t A_rank, size_t A_size>
	struct t_of
	{
		std::atomic<T*> v_chunks;
		std::atomic_size_t v_grown;
		std::atomic_size_t v_allocated;
		std::atomic_size_t v_returned;
		size_t v_freed = 0;

		T* f_grow(t_heap& a_heap)
		{
			auto size = c_UNIT << A_rank;
			auto length = size * A_size;
			auto block = static_cast<char*>(f_map(length));
			auto p = block;
			for (size_t i = 1; i < A_size; ++i) {
				auto q = new(p) T(A_rank);
				q->v_next = reinterpret_cast<T*>(p += size);
			}
			new(p) T(A_rank);
			auto q = reinterpret_cast<T*>(block);
			q->v_chunk_size = A_size;
			{
				std::lock_guard lock(a_heap.v_mutex);
				a_heap.v_blocks.emplace(q, length);
			}
			v_grown.fetch_add(A_size, std::memory_order_relaxed);
			a_heap.v_tick();
			return q;
		}
		T* f_allocate(t_heap& a_heap)
		{
			auto p = v_chunks.load(std::memory_order_acquire);
			while (p && !v_chunks.compare_exchange_weak(p, p->v_chunk_next, std::memory_order_acquire));
			if (!p) p = f_grow(a_heap);
			v_allocated.fetch_add(p->v_chunk_size, std::memory_order_relaxed);
			return p;
		}
		void f_return(size_t a_n)
		{
			auto p = v_head<A_rank>;
			p->v_chunk_size = a_n;
			p->v_chunk_next = v_chunks.load(std::memory_order_relaxed);
			while (!v_chunks.compare_exchange_weak(p->v_chunk_next, p, std::memory_order_release));
			v_head<A_rank> = nullptr;
			v_returned.fetch_add(a_n, std::memory_order_relaxed);
		}
		void f_return()
		{
			size_t n = 0;
			for (auto p = v_head<A_rank>; p; p = p->v_next) ++n;
			if (n > 0) f_return(n);
		}
		void f_flush()
		{
			f_return(v_freed);
			v_freed = 0;
		}
		void f_free(T* a_p)
		{
			a_p->v_next = v_head<A_rank>;
			v_head<A_rank> = a_p;
			if (++v_freed >= A_size) f_flush();
		}
		size_t f_live() const
		{
			return v_allocated.load(std::memory_order_relaxed) - v_returned.load(std::memory_order_relaxed) - v_freed;
		}
	};

	template<size_t A_rank>
	static inline RECYCLONE__THREAD T* v_head;

	void(*v_tick)();
	std::map<T*, size_t> v_blocks;
	std::mutex v_mutex;
	t_of<0, 1024 * 64> v_of0;
	t_of<1, 1024 * 16> v_of1;
	t_of<2, 1024 * 4> v_of2;
	t_of<3, 1024> v_of3;
	t_of<4, 1024 / 4> v_of4;
	t_of<5, 1024 / 16> v_of5;
	t_of<6, 1024 / 64> v_of6;
	size_t v_allocated = 0;
	size_t v_freed = 0;

	template<size_t A_rank, size_t A_size>
	T* f_allocate_from(t_of<A_rank, A_size>& a_of);
	template<size_t A_rank, size_t A_size>
	RECYCLONE__ALWAYS_INLINE T* f_allocate(t_of<A_rank, A_size>& a_of)
	{
		auto p = v_head<A_rank>;
		if (!p) [[unlikely]] return f_allocate_from(a_of);
		v_head<A_rank> = p->v_next;
		return p;
	}
	T* f_allocate_large(size_t a_size);
	constexpr T* f_allocate_medium(size_t a_size);

public:
	static constexpr size_t c_UNIT = std::bit_floor(sizeof(T)) << 1;
	static_assert(c_UNIT >> 1 <= sizeof(T));
	static_assert(c_UNIT > sizeof(T));
	static constexpr size_t c_RANKX = std::countl_zero(c_UNIT - 1);
	static_assert((c_UNIT << (c_RANKX - 1)) - 1 == ~size_t(0) >> 1);
	static_assert((c_UNIT << c_RANKX) - 1 == ~size_t(0));

	t_heap(void(*a_tick)()) : v_tick(a_tick)
	{
	}
	~t_heap()
	{
		for (auto& x : v_blocks) f_unmap(x.first, x.second);
	}
	size_t f_live() const
	{
		return v_of0.f_live() + v_of1.f_live() + v_of2.f_live() + v_of3.f_live() + v_of4.f_live() + v_of5.f_live() + v_of6.f_live() + v_allocated - v_freed;
	}
	void f_statistics(auto a_each) const
	{
		a_each(size_t(0), v_of0.v_grown.load(std::memory_order_relaxed), v_of0.v_allocated.load(std::memory_order_relaxed), v_of0.v_returned.load(std::memory_order_relaxed));
		a_each(size_t(1), v_of1.v_grown.load(std::memory_order_relaxed), v_of1.v_allocated.load(std::memory_order_relaxed), v_of1.v_returned.load(std::memory_order_relaxed));
		a_each(size_t(2), v_of2.v_grown.load(std::memory_order_relaxed), v_of2.v_allocated.load(std::memory_order_relaxed), v_of2.v_returned.load(std::memory_order_relaxed));
		a_each(size_t(3), v_of3.v_grown.load(std::memory_order_relaxed), v_of3.v_allocated.load(std::memory_order_relaxed), v_of3.v_returned.load(std::memory_order_relaxed));
		a_each(size_t(4), v_of4.v_grown.load(std::memory_order_relaxed), v_of4.v_allocated.load(std::memory_order_relaxed), v_of4.v_returned.load(std::memory_order_relaxed));
		a_each(size_t(5), v_of5.v_grown.load(std::memory_order_relaxed), v_of5.v_allocated.load(std::memory_order_relaxed), v_of5.v_returned.load(std::memory_order_relaxed));
		a_each(size_t(6), v_of6.v_grown.load(std::memory_order_relaxed), v_of6.v_allocated.load(std::memory_order_relaxed), v_of6.v_returned.load(std::memory_order_relaxed));
		a_each(size_t(c_RANKX), size_t(0), v_allocated, v_freed);
	}
	RECYCLONE__ALWAYS_INLINE constexpr T* f_allocate(size_t a_size)
	{
		if (a_size <= c_UNIT) [[likely]] return f_allocate(v_of0);
		return f_allocate_medium(a_size);
	}
	void f_return()
	{
		v_of0.f_return();
		v_of1.f_return();
		v_of2.f_return();
		v_of3.f_return();
		v_of4.f_return();
		v_of5.f_return();
		v_of6.f_return();
	}
	void f_flush()
	{
		if (v_of0.v_freed > 0) v_of0.f_flush();
		if (v_of1.v_freed > 0) v_of1.f_flush();
		if (v_of2.v_freed > 0) v_of2.f_flush();
		if (v_of3.v_freed > 0) v_of3.f_flush();
		if (v_of4.v_freed > 0) v_of4.f_flush();
		if (v_of5.v_freed > 0) v_of5.f_flush();
		if (v_of6.v_freed > 0) v_of6.f_flush();
	}
	void f_free(T* a_p)
	{
		switch (a_p->v_rank) {
		case 0:
			v_of0.f_free(a_p);
			break;
		case 1:
			v_of1.f_free(a_p);
			break;
		case 2:
			v_of2.f_free(a_p);
			break;
		case 3:
			v_of3.f_free(a_p);
			break;
		case 4:
			v_of4.f_free(a_p);
			break;
		case 5:
			v_of5.f_free(a_p);
			break;
		case 6:
			v_of6.f_free(a_p);
			break;
		default:
			v_mutex.lock();
			auto i = v_blocks.find(a_p);
			auto n = i->second;
			v_blocks.erase(i);
			v_mutex.unlock();
			f_unmap(a_p, n);
			++v_freed;
		}
	}
	const std::map<T*, size_t>& f_blocks() const
	{
		return v_blocks;
	}
	std::mutex& f_mutex()
	{
		return v_mutex;
	}
	T* f_find(void* a_p)
	{
		auto i = v_blocks.lower_bound(static_cast<T*>(a_p));
		if (i == v_blocks.end() || i->first != a_p) {
			if (i == v_blocks.begin()) return nullptr;
			--i;
		}
		size_t j = static_cast<char*>(a_p) - reinterpret_cast<char*>(i->first);
		return j < i->second && (j & (c_UNIT << i->first->v_rank) - 1) == 0 ? static_cast<T*>(a_p) : nullptr;
	}
};

template<typename T>
template<size_t A_rank, size_t A_size>
T* t_heap<T>::f_allocate_from(t_of<A_rank, A_size>& a_of)
{
	auto p = a_of.f_allocate(*this);
	v_head<A_rank> = p->v_next;
	return p;
}

template<typename T>
T* t_heap<T>::f_allocate_large(size_t a_size)
{
	auto p = new(f_map(a_size)) T(c_RANKX);
	std::lock_guard lock(v_mutex);
	v_blocks.emplace(p, a_size);
	++v_allocated;
	return p;
}

template<typename T>
constexpr T* t_heap<T>::f_allocate_medium(size_t a_size)
{
	if (a_size <= c_UNIT << 1) return f_allocate(v_of1);
	if (a_size <= c_UNIT << 2) return f_allocate(v_of2);
	if (a_size <= c_UNIT << 3) return f_allocate(v_of3);
	if (a_size <= c_UNIT << 4) return f_allocate(v_of4);
	if (a_size <= c_UNIT << 5) return f_allocate(v_of5);
	if (a_size <= c_UNIT << 6) return f_allocate(v_of6);
	return f_allocate_large(a_size);
}

}

#endif
