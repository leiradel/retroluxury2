#include "log.h"

static void hh2_dummyLogger(hh2_LogLevel level, char const* format, va_list ap) {
    (void)level;
    (void)format;
    (void)ap;
}

static hh2_Logger hh2_logger = hh2_dummyLogger;

void hh2_setLogger(hh2_Logger logger) {
    hh2_logger = logger;
}

void hh2_log(hh2_LogLevel level, char const* format, ...) {
    va_list ap;
    va_start(ap, format);
    hh2_logger(level, format, ap);
    va_end(ap);
}

void hh2_vlog(hh2_LogLevel level, char const* format, va_list ap) {
    hh2_logger(level, format, ap);
}
