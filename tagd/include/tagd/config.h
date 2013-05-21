#ifndef TAGD_CONFIG
#define TAGD_CONFIG

#ifdef __cplusplus

#ifdef DEBUG
#include <iostream>
// iostream debug
#define dout std::cerr
#else
#define dout if(0) std::cerr
#endif

#endif

#endif
