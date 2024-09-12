/**
 * Copyright (c) 2020 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef _TWIN_LOGGING_H_
#define _TWIN_LOGGING_H_

#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

typedef struct {
    va_list ap;
    const char *fmt;
    const char *file;
    struct tm *time;
    void *udata;
    int line;
    int level;
} log_event_t;

typedef void (*log_func_t)(log_event_t *ev);
typedef void (*log_lock_func_t)(bool lock, void *udata);

enum { LOGC_TRACE, LOGC_DEBUG, LOGC_INFO, LOGC_WARN, LOGC_ERROR, LOGC_FATAL };

#if defined(CONFIG_LOGGING)
#define log_trace(...) log_impl(LOGC_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_impl(LOGC_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...) log_impl(LOGC_INFO, __FILE__, __LINE__, __VA_ARGS__)

#else /* !CONFIG_LOGGING */
#define log_trace(...) \
    do {               \
    } while (0)
#define log_debug(...) \
    do {               \
    } while (0)
#define log_info(...) \
    do {              \
    } while (0)
#endif /* CONFIG_LOGGING */

#define log_warn(...) log_impl(LOGC_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_impl(LOGC_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_impl(LOGC_FATAL, __FILE__, __LINE__, __VA_ARGS__)

const char *log_level_string(int level);
void log_set_lock(log_lock_func_t fn, void *udata);
void log_set_level(int level);
void log_set_quiet(bool enable);
int log_add_callback(log_func_t fn, void *udata, int level);
int log_add_fp(FILE *fp, int level);

#if defined(CONFIG_LOGGING)
void log_impl(int level, const char *file, int line, const char *fmt, ...);
#else
#define log_impl(...)              \
    do {                           \
        /* dummy implementation */ \
    } while (0)
#endif

#endif /* _TWIN_LOGGING_H_ */
