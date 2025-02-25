#ifndef GEMINO_DEBUG_HPP
#define GEMINO_DEBUG_HPP

#include <iostream>
#include <chrono>

#define STRINGIFY(x) #x

#ifdef NDEBUG
#define DEBUG_MODE 0
#else
#define DEBUG_MODE 1
#endif

#define DEBUG_LOG(message) { std::cout << "\033[97m" << message << "\033[0\n"; }
#define DEBUG_WARNING(message) { std::cout << "\033[93m" << message << "\033[0\n"; }
#define DEBUG_ERROR(message) { std::cout << "\033[91m" << message << "\033[0\n"; }

#define DEBUG_PANIC(message) { DEBUG_ERROR( __FILE__ << ":" << __LINE__ << ": " << message); exit(1); }
#define DEBUG_ASSERT(x)  if (!(x)) DEBUG_PANIC("Expression failed: " << #x);
#define DEBUG_CONST_ASSERT(x)  if constexpr(!(x)) DEBUG_PANIC("Expression failed: " << #x);

#define _DEBUG_TIMESTAMP_NAME(name) __timestamp##name
#define DEBUG_TIMESTAMP(name) auto _DEBUG_TIMESTAMP_NAME(name) = std::chrono::high_resolution_clock::now()
#define DEBUG_TIME_DIFF(a, b) std::chrono::duration<double>(_DEBUG_TIMESTAMP_NAME(b) - _DEBUG_TIMESTAMP_NAME(a)).count()

#endif