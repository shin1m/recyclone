#ifndef TEST__DEFINE_H
#define TEST__DEFINE_H

#include <recyclone/engine.h>
#ifdef NDEBUG
#undef NDEBUG
#include <cassert>
#define NDEBUG
#endif

using namespace recyclone;

#ifdef _WIN32
inline const auto v_suppress_abort_message = _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif

#endif
