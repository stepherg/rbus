#ifndef RT_STRING_UTILS_H
#define RT_STRING_UTILS_H

#include <string.h>
#include <stddef.h>

/*
 * Safe, lightweight string copy similar to BSD strlcpy semantics:
 *  - Always NUL terminates if dstsz > 0
 *  - Copies up to dstsz-1 bytes
 *  - Returns number of bytes copied (not including NUL)
 */
static inline size_t rt_strlcpy(char *dst, const char *src, size_t dstsz)
{
    if(!dst || !src || dstsz == 0)
        return 0;
    size_t n = strnlen(src, dstsz - 1);
    memcpy(dst, src, n);
    dst[n] = '\0';
    return n;
}

#endif /* RT_STRING_UTILS_H */
