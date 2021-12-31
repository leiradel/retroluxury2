#ifndef HH2_LOG_H__
#define HH2_LOG_H__

#include <stdarg.h>

typedef enum {
    HH2_LOG_DEBUG,
    HH2_LOG_INFO,
    HH2_LOG_WARN,
    HH2_LOG_ERROR
}
hh2_LogLevel;

typedef void (*hh2_Logger)(hh2_LogLevel level, char const* format, va_list ap);

void hh2_setLogger(hh2_Logger logger);

#ifdef HH2_ENABLE_LOGGING
    #define HH2_LOG(...) do { hh2_log(__VA_ARGS__); } while (0)
    #define HH2_VLOG(level, format, ap) do { hh2_vlog(level, format, ap); } while (0)

    void hh2_log(hh2_LogLevel level, char const* format, ...);
    void hh2_vlog(hh2_LogLevel level, char const* format, va_list ap);
#else
    #define HH2_LOG(...) do {} while (0)
    #define HH2_VLOG(level, format, ap) do {} while (0)
#endif // HH2_ENABLE_LOGGING

#endif // HH2_LOG_H__
