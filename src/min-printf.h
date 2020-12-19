#ifndef _MIN_PRINTF_H
#define _MIN_PRINTF_H

#ifdef DEBUG_MESSAGES

#include <stdarg.h>

#define puts(...) _puts(0, __VA_ARGS__)
int _puts(int fd, const char *str);
int _vfprintf(int fd, const char *format, va_list args);
int _fprintf(int fd, const char *format, ...) __attribute__ ((__format__ (__printf__, 2, 3)));
int printf(const char *format, ...) __attribute__ ((__format__ (__printf__, 1, 2)));

#endif

#endif /* _MIN_PRINTF_H */
