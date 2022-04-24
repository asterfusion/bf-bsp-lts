/*!
 * @file pltfm_types.h
 * @date 2021/07/06
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

#ifndef _AF_PLTFM_TYPES_H
#define _AF_PLTFM_TYPES_H

#ifdef INC_PLTFM_UCLI
#include <bfutils/uCli/ucli.h>
#endif

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

#ifndef VALID
#define VALID 1
#define INVALID 0
#endif

#ifndef FOREVER
#define FOREVER for(;;)
#endif

#ifndef THIS_API_IS_ONLY_FOR_TEST
#define THIS_API_IS_ONLY_FOR_TEST
#endif

#ifndef The_interface_has_not_been_tested
#define The_interface_has_not_been_tested
#define THE_INTERFACE_HAS_NOT_BEEN_TESTED
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef EQ
#define EQ(a, b) (!(((a) > (b)) - ((a) < (b))))
#endif
#ifndef GT
#define GT(a, b) ((a) > (b))
#endif
#ifndef LT
#define LT(a, b) ((a) < (b))
#endif

/** Alway treated the expr as true */
#ifndef likely
#define likely(expr) __builtin_expect(!!(expr), 1)
#endif

/** Alway treated the expr as false */
#ifndef unlikely
#define unlikely(expr) __builtin_expect(!!(expr), 0)
#endif

#define BIT(n) (n)
#define MASKBIT(v,b) ((v) |= (1 << (b)))
#define IGNOREBIT(v,b) ((v) &= ~(1 << (b)))

#ifndef BUG_ON
#define BUG_ON(expr)    assert(!(expr))
#endif

#ifndef WARN_ON
#define WARN_ON(expr)   assert(!(expr))
#endif

#ifndef TRUE
#define TRUE true
#endif

#ifndef FALSE
#define FALSE false
#endif

#ifndef EXPORT
#define EXPORT
#endif

#ifndef IN
#define IN  /* Parameter IN, always be a constant */
#endif

#ifndef OUT
#define OUT /* Parameter OUT, always be a pointer */
#endif

#ifndef IO
#define IO  /* Parameter IN and OUT, means that an input pointer maybe modified. */
#endif

#ifndef RDONLY
#define RDONLY
#endif

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(a) (int)(sizeof(a)/sizeof((a)[0]))
#endif

#ifndef __BITS_PER_LONG
#if defined(__x86_64__) || defined(__aarch64__)
#   define __BITS_PER_LONG 64
#else
#   define __BITS_PER_LONG 32
#endif
#endif

#if 0
#ifndef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE,MEMBER) __compiler_offsetof(TYPE,MEMBER)
#else
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif
#endif

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:        the pointer to the member.
 * @type:       the type of the container struct this is embedded in.
 * @member:     the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({ \
            const typeof(((type *)0)->member) *__mptr = (ptr); \
            (type *)((char *)__mptr - offsetof(type,member));})
#endif

#define LOG_DIR_PREFIX  "/var/asterfusion"

#define BOLD        "\e[1m"
#define UNDERLINE   "\e[4m"
#define BLINK       "\e[5m"
#define REVERSE     "\e[7m"
#define HIDE        "\e[8m"
#define CLEAR       "\e[2J"
#define CLRLINE     "\r\e[K" //or "\e[1K\r"

#define foreach_color           \
  _(FIN,    "\e[0m",    "Finish drawing.") \
  _(BLACK,  "\e[0;30m", "Black color.") \
  _(RED,    "\e[0;31m", "Red color.") \
  _(GREEN,  "\e[0;32m", "Green color.") \
  _(YELLOW, "\e[0;33m", "Yellow color.") \
  _(BLUE,   "\e[0;34m", "Blue color.") \
  _(PURPLE, "\e[0;35m", "Purple color.") \
  _(CYAN,   "\e[0;36m", "Cyan color.") \
  _(WHITE,  "\e[0;37m", "White color.")

typedef enum {
#define _(f,s,d) COLOR_##f,
    foreach_color
#undef _
    COLORS,
} color_t;


#define ASSERT_ENABLE   1

#define ASSERT(expr)                    \
do {                                    \
  if (ASSERT_ENABLE && ! (expr))        \
    {                                   \
      fprintf (stdout,                  \
           "%s:%lu (%s) assertion `%s' fails\n",    \
           __FILE__,                    \
           (unsigned long) __LINE__,                \
           ((const char *) __FUNCTION__),           \
           # expr);                                 \
    }                                               \
} while (0)


static inline const char *
oryx_safe_strerror
(
    IN int err
)
{
    const char *s = strerror (err);
    return (s != NULL) ? s : "Unknown error";
}


#define foreach_element(minv,maxv)\
        for (int each_element = (int)minv; each_element < (int)maxv;\
         each_element ++)

#ifdef __cplusplus
}
#endif /* C++ */
#endif /* _BF_PLTFM_UART_H */
