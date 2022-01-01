#ifndef RL2_LOG_H__
#define RL2_LOG_H__

#include <stdarg.h>

typedef enum {
    RL2_LOG_DEBUG,
    RL2_LOG_INFO,
    RL2_LOG_WARN,
    RL2_LOG_ERROR
}
rl2_LogLevel;

typedef void (*rl2_Logger)(rl2_LogLevel level, char const* format, va_list ap);

void rl2_setLogger(rl2_Logger logger);

#ifdef RL2_BUILD_DEBUG
void rl2_log(rl2_LogLevel level, char const* file, unsigned line, const const* function, char const* format, ...);
#else
void rl2_log(rl2_LogLevel level, char const* format, ...);
#endif

#ifdef RL2_ENABLE_LOG_DEBUG
    #ifdef RL2_BUILD_DEBUG
        #define RL2_DEBUG(...) do { rl2_log(RL2_LOG_DEBUG, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__); } while (0)
    #else
        #define RL2_DEBUG(...) do { rl2_log(RL2_LOG_DEBUG, __VA_ARGS__); } while (0)
    #endif
#else
    #define RL2_DEBUG(...)
#endif

#ifdef RL2_BUILD_DEBUG
    #define RL2_INFO(...) do { rl2_log(RL2_LOG_INFO, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__); } while (0)
    #define RL2_WARN(...) do { rl2_log(RL2_LOG_WARN, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__); } while (0)
    #define RL2_ERROR(...) do { rl2_log(RL2_LOG_ERROR, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__); } while (0)
#else
    #define RL2_INFO(...) do { rl2_log(RL2_LOG_INFO, __VA_ARGS__); } while (0)
    #define RL2_WARN(...) do { rl2_log(RL2_LOG_WARN, __VA_ARGS__); } while (0)
    #define RL2_ERROR(...) do { rl2_log(RL2_LOG_ERROR, __VA_ARGS__); } while (0)
#endif

#endif // RL2_LOG_H__
