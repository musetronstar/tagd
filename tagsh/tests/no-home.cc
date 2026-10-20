#include <pwd.h>

// Exercise startup without a passwd home even when the test runner has one.
extern "C" struct passwd* getpwuid(uid_t) {
	return nullptr;
}
