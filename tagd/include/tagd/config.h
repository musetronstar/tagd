#ifndef TAGD_CONFIG
#define TAGD_CONFIG

    /*|                                            |*\
    |*|  macros for std::cerr and std::cout        |*|
    |*|  should be used so they can be traced      |*|
    \*|                                            |*/

#define TAGD_COUT std::cout
#define TAGD_CERR std::cerr

/*\
|*|    RFC 5424     The Syslog Protocol      March 2009
|*|    ------------------------------------------------
|*|
|*|    Numerical         Severity
|*|      Code
|*|
|*|       0       Emergency: system is unusable
|*|       1       Alert: action must be taken immediately
|*|       2       Critical: critical conditions
|*|       3       Error: error conditions
|*|       4       Warning: warning conditions
|*|       5       Notice: normal but significant condition
|*|       6       Informational: informational messages
|*|       7       Debug: debug-level messages
\*/

#define LOG_ERROR(MSG) std::cerr << MSG ;
#define LOG_DEBUG(MSG) std::cerr << MSG ;

// LOG_TRACE() is DEBUG level __FILE__ and __LINE__ prepended to message
// MUST be defined in namespace where it is called

#endif // TAGD_CONFIG
