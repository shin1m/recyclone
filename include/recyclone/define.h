#ifndef RECYCLONE__DEFINE_H
#define RECYCLONE__DEFINE_H

#ifdef __GNUC__
#define RECYCLONE__THREAD __thread
#define RECYCLONE__ALWAYS_INLINE __attribute__((always_inline))
#define RECYCLONE__NOINLINE __attribute__((noinline))
#define RECYCLONE__FORCE_INLINE
#endif
#ifdef _MSC_VER
#define RECYCLONE__THREAD __declspec(thread)
#define RECYCLONE__ALWAYS_INLINE
#define RECYCLONE__NOINLINE __declspec(noinline)
#define RECYCLONE__FORCE_INLINE __forceinline
#endif
#ifdef __EMSCRIPTEN__
// Workaround to spill pointers.
#define RECYCLONE__SPILL volatile
#else
#define RECYCLONE__SPILL
#endif

#ifndef RECYCLONE__COOPERATIVE
#ifdef __unix__
#ifndef RECYCLONE__SIGNAL_SUSPEND
#define RECYCLONE__SIGNAL_SUSPEND SIGRTMAX - 1
#endif
#ifndef RECYCLONE__SIGNAL_RESUME
#define RECYCLONE__SIGNAL_RESUME SIGRTMAX
#endif
#endif
#endif

#endif
