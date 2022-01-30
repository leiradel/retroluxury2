#include "rl2_log.h"

#include <stdio.h>

static void rl2_dummyLogger(rl2_LogLevel level, char const* format, va_list ap) {
    (void)level;
    (void)format;
    (void)ap;
}

static rl2_Logger rl2_logger = rl2_dummyLogger;

void rl2_setLogger(rl2_Logger const logger) {
    rl2_logger = logger;
}

#ifdef RL2_BUILD_DEBUG
void rl2_log(rl2_LogLevel level, char const* file, unsigned line, char const* format, ...) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s:%u: %s", file, line, format);

    va_list ap;
    va_start(ap, format);
    rl2_logger(level, buffer, ap);
    va_end(ap);
}
#else
void rl2_log(rl2_LogLevel level, char const* format, ...) {
    va_list ap;
    va_start(ap, format);
    rl2_logger(level, format, ap);
    va_end(ap);
}
#endif
