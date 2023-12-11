#ifndef GEMINO_DEBUG_HPP
#define GEMINO_DEBUG_HPP

#include <iostream>

#define STRINGIFY(x) #x

#define DEBUG_LOG(message) { std::cout << "\033[97m" << message << "\033[0\n"; }
#define DEBUG_WARNING(message) { std::cout << "\033[93m" << message << "\033[0\n"; }
#define DEBUG_ERROR(message) { std::cout << "\033[91m" << message << "\033[0\n"; }

#define DEBUG_PANIC(message) { DEBUG_ERROR( __FILE__ << ":" << __LINE__ << ": " << message); exit(1); }
#define DEBUG_ASSERT(x)  if (!(x)) DEBUG_PANIC("Expression failed: " << #x);

#endif