// The engine is built without exceptions; libc++'s operator new still names these, so give them a stop.
#include <cstdlib>
extern "C" void *__cxa_allocate_exception(unsigned long) { abort(); }
extern "C" void __cxa_throw(void *, void *, void (*)(void *)) { abort(); }
