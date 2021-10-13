#ifndef PK_H
#define PK_H

/* Attie's printf() / printk() support macros.
 *
 * These are intended only for development purposes, and are not considered
 * suitable for use in production-ready code. This header file should perform
 * correctly when used from both within the Linux kernel, as well as userspace.
 *
 * Effort has been made to reduce the processing required at run-time, in an
 * attempt at reducing unwanted side effects.
 */

/* -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=-
 * CONFIGURATION:
 */

/* Optionally define PK_LEVEL in kernel space or U-Boot.
 */
#ifndef PK_LEVEL
# if defined(__KERNEL__)
#   define PK_LEVEL KERN_INFO
# endif
#endif

/* Optionally define PK_FUNC to redirect the message output. The arguments (fmt
 * and args...) are equivelant to those passed to printf().
 */
#ifndef PK_FUNC
# if defined(__KERNEL__) && defined(__UBOOT__)
    /* U-Boot */
#   define PK_FUNC(fmt, args...)  printk(PK_LEVEL fmt "\n", ##args)
# elif defined(__KERNEL__) && defined(__linux__)
    /* Linux Kernel */
#   define PK_FUNC(fmt, args...)  printk(PK_LEVEL fmt,      ##args)
# else
    /* Userspace */
#   define PK_FUNC(fmt, args...) fprintf(stderr,  fmt "\n", ##args)
# endif
#endif

/* Optionally define PK_TAG to label the messages. Every message will contain
 * this text to support better filtering of any messages generated.
 */
#ifndef PK_TAG
# define PK_TAG "ATTIE"
#endif

/* Optionally define PK_DUMP_WIDTH to adjust the width of hexdump output, in
 * bytes.
 */
#ifndef PK_DUMP_WIDTH
# define PK_DUMP_WIDTH 16
#endif

/* -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=-
 * INTERNAL / SUPPORT:
 */

#ifndef __KERNEL__
# include <stdio.h>
# include <stdint.h>
# include <string.h>
# include <errno.h>
#else
# include <linux/kernel.h>
# include <linux/printk.h>
#endif

/* Stringification */
#define _PKS2(x) #x
#define _PKS(x) _PKS2(x)

/* These macros are used to build static strings containing the filename and
 * line number. _PKFL expands to the string "${filename}:${lineno}".
 */
#define _PKFL __FILE__ ":" _PKS(__LINE__)

/* These macros are the fundamental building blocks of this functionality. They
 * are not intended for use directly from user code.
 *
 *   - _PK()  - Add the tag, file name, line number and function name to the
 *              message, before passing to PK_FUNC().
 *   - _PKT() - Add the timer's tag and timestamp to the message, before passing
 *              to _PK() and ultimately PK_FUNC().
 */
#define _PK(fmt, args...) PK_FUNC(PK_TAG ": " _PKFL " %s()" fmt, __func__, ##args)
#define _PKT(tag, ts, fmt, args...) _PK(": " tag " @ %ld.%09ld" fmt, ts.tv_sec, ts.tv_nsec, ##args);

/* These macros take care of time-based calculations. They can be used from user
 * code if necessary.
 *
 *   - _PKTADD()  - Add a to b, and store the result in b.
 *   - _PKTSUB()  - Subtract a from b, and store the result in b.
 *   - _PKTDIFF() - Put the difference between start and "now" into the diff
 *                  argument.
 *   - _PKTACC()  - Accumulate the difference between start and "now" into the
 *                  acc argument.
 */
#define _PKTADD(a, b)                             \
  {                                               \
    b.tv_nsec += a.tv_nsec; b.tv_sec += a.tv_sec; \
    if (b.tv_nsec >= 1000000000)                  \
      { b.tv_sec += 1; b.tv_nsec -= 1000000000; } \
  }

#define _PKTSUB(a, b)                             \
  {                                               \
    if (b.tv_nsec < a.tv_nsec)                    \
      { b.tv_sec -= 1; b.tv_nsec += 1000000000; } \
    b.tv_sec -= a.tv_sec; b.tv_nsec -= a.tv_nsec; \
  }

#define _PKTDIFF(start, diff) \
  {                           \
    PKTSTART(diff);           \
    _PKTSUB(start, diff);     \
  }

#define _PKTACC(start, acc) \
  {                         \
    struct timespec _t;     \
    PKTSTART(_t);           \
    _PKTDIFF(start, _t);    \
    _PKTADD(_t, acc);       \
  }

/* This function takes care of producing a hex-dump. It should not be used from
 * user code. PK_FUNC must be called directly to maintain the file, line number
 * and function name from the original location the macro was used.
 */
static inline void _pk_dump(const char *_pkfl, const char *_pkfn, const void *_data, size_t len) {
	const uint8_t *data = (const uint8_t *)_data;
	size_t i, o;
	char buf_hex  [(PK_DUMP_WIDTH * 3) + 1];
	char buf_print[ PK_DUMP_WIDTH      + 1];

	for (i = 0; (data != NULL) && (i < len); i++) {
		o = i % PK_DUMP_WIDTH;

		snprintf(&(buf_hex[o * 3]), 4, " %02hhx", data[i]);
		buf_print[o] = ((data[i] >= ' ') && (data[i] <= '~')) ? data[i] : '.';

		if ((o < (PK_DUMP_WIDTH - 1)) && (i < (len - 1))) continue;

		PK_FUNC(PK_TAG ": %s %s(): DUMP: 0x%04zx:%-*.*s | %.*s",
			_pkfl, _pkfn, i - o,
			PK_DUMP_WIDTH * 3, (int)((o+1) * 3), buf_hex,
			                   (int) (o+1),      buf_print
		);
	}
}

/* This function takes care of splitting a buffer into multiple chunks,
 * delimited by `c`. On the first call, `*cookie` must be NULL. The returned
 * pointer indicates the start of the chunk, and `*chunklen` indicates the
 * length of that chunk. The buffer is never modified, so when passing the chunk
 * to printf() or similar, a "%.*s" format string should be used. Zero length
 * chunkss (i.e: "\n\n") are maintained. */
static inline const char *_pk_nextchunk(const char *buf, size_t len, const char **cookie, size_t *chunklen, const char c) {
	const char *start, *end;

	if ((buf == NULL) || (len == 0) || (cookie == NULL)) return NULL;
	if (*cookie == NULL) *cookie = buf;

	start = *cookie; end = buf + len;

	if ((start < buf) || (start >= end) || (**cookie == '\0')) return NULL;
	while ((*cookie < end) && (**cookie != '\0') && (**cookie != c)) *cookie += 1;

	if (chunklen != NULL) *chunklen = *cookie - start;
	if (**cookie == c) *cookie += 1;

	return start;
}

/* -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=-
 * GENERIC MESSAGES:
 */

#define _PKV_fmt(fmt, var)   #var ": " fmt
#define _PKV_var(fmt, var)   var
#define _PKVN_sep()          ",  "
#define _PKVN_fmt(args...)   MAP_PAIRS(_PKV_fmt,  _PKVN_sep, ##args)
#define _PKVN_var(args...)   MAP_PAIRS(_PKV_var,  COMMA,     ##args)

/* These macros are the intended public interface for generic messages, and
 * should be used from within your application.
 *
 *   - PK()      - Print the base message only, with no additional content.
 *   - PKS()     - Print the base message, with a static string. Note that this
 *                 does not use string concatenation to avoid accidental use of
 *                 format specifiers in the static string.
 *   - PKF()     - Print the base message, with a fully-formed format string and
 *                 associated arguments.
 *   - PKV()     - Print the name and value of a variable using the given format
 *                 string. Multiple variables may be presented by giving more
 *                 than one format / variable pair. It can be useful to include
 *                 square brackets around a string format to show the start and
 *                 end clearly.
 *   - PKE()     - Print the given message, suffixed with the errno and relevant
 *                 string description, if available - like perror().
 */

#define PK()               _PK("")
#define PKS(str)           _PK(": %s", str)
#define PKF(fmt, args...)  _PK(": " fmt, ##args)
#define PKV(fmt_arg...)    _PK(": " _PKVN_fmt(fmt_arg),  _PKVN_var(fmt_arg))

#if !defined(__KERNEL__)
/* user-space gets access to strerror_r(), and can thus try to be more descriptive */
# if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && !defined(_GNU_SOURCE)
    /* XSI strerror_r() */
#   define _PKE_SE(e, s, l) { int _r = strerror_r(e, s, l); if (_r != 0) s = ""; }
# else
    /* GNU strerror_r() */
#   define _PKE_SE(e, s, l) { char *_r = strerror_r(e, s, l); s = _r == NULL ? "" : _r; }
# endif
# define PKE(fmt, args...)                                         \
   {                                                               \
     int _e = errno; char _s[1024]; char *_c = &(_s[0]);           \
     _PKE_SE(_e, _c, sizeof(_s));                                  \
     if (_c[0] == '\0') _c = "Unknown error";                      \
     _PK(": " fmt ": %d / %.*s", ##args, _e, (int)sizeof(_s), _c); \
     errno = _e;                                                   \
   }
#else
/* kernel-space does not have access to strerror_r() */
# define PKE(fmt, args...)             \
   {                                   \
     int _e = errno;                   \
     _PK(": " fmt ": %d", ##args, _e); \
     errno = _e;                       \
   }
#endif

#define PKR(ret_type, ret_fmt, op) \
  ({ \
    ret_type _ret = ( op ); \
    PKF(#op " --> " ret_fmt, _ret); \
    _ret; \
  })

/* -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=-
 * TIME-BASED MESSAGES:
 */

/* These macros are the intended public interface for time-based messages, and
 * should be used from within your application.
 *
 *   - PKTSTART() - Retrieve the current monotonic timestamp, and store it in
 *                  ts. It should be called before PKTDIFF() and PKTACC() to
 *                  acquire a starting timestamp.
 *                  The `ts` argument is expected to be a `struct timespec`.
 *   - PKTSTAMP() - Print the current timestamp, with "TSTAMP" in the generated
 *                  message. The generated message has time specified to nano-
 *                  seconds, but this resolution may not be technically
 *                  attainable on your system.
 *   - PKTDIFF()  - Print the difference in time between a timestamp that was
 *                  previously taken with PKTSTART(), and now. "TDIFF" is present
 *                  in the generated message.
 *   - PKTACC()   - Accumulate the difference in time between a previously
 *                  acquired timestamp `ts`, and "now" into `acc`, and then
 *                  print the running total. "TACC" is present in the generated
 *                  message.
 *   - PKTRATE()  - Calculate the difference in time between a timestamp that
 *                  was previously taken with PKTSTART(), and then print a
 *                  message conveying the number of items, time taken and the
 *                  calculated frequency. "TRATE" is present in the generated
 *                  message.
 *   - PKTRAW()   - Print the raw timestamp given in `ts`. This can be used
 *                  to print a timestamp acquired with PKTSTART(), or print
 *                  a summary / final value for PKTACC() after the event. "TRAW"
 *                  is present in the generated message.
 *   - PKTRAWS()  - The same as PKTRAW(), but with a static string - see PK()
 *                  vs. PKS(). "TRAWS" is present in the generated message.
 *   - PKTRAWF()  - The same as PKTRAW(), but with a format string - see PK()
 *                  vs. PKF(). "TRAWF" is present in the generated message.
 */
#ifdef __KERNEL__
# define PKTSTART(ts) getrawmonotonic(&(ts))
#else
# include <time.h>
# define PKTSTART(ts)                                                \
  {                                                                  \
    int _ret;                                                        \
    if ((_ret = clock_gettime(CLOCK_REALTIME, &(ts))) != 0)          \
      PKF("PKTSTART: clock_gettime(&" #ts ") returned %d...", _ret); \
  }
#endif

#define PKTSTAMP(fmt, args...)             \
  {                                        \
    struct timespec _t; PKTSTART(_t);      \
    _PKT("TSTAMP", _t, ": " fmt, ##args);  \
  }

#define PKTDIFF(ts, fmt, args...)                 \
  {                                               \
    struct timespec _t; _PKTDIFF(ts, _t);         \
    _PKT("TDIFF(" #ts ")", _t, ": " fmt, ##args); \
  }

#define PKTACC(ts, acc, fmt, args...)              \
  {                                                \
    _PKTACC(ts, acc);                              \
    _PKT("TACC(" #acc ")", acc, ": " fmt, ##args); \
  }

#define PKTRATE(ts, n, fmt, args...)                             \
  {                                                              \
    struct timespec _t; double _f;                               \
    _PKTDIFF(ts, _t);                                            \
    _f = n / (_t.tv_sec + (_t.tv_nsec / 1000000000.0f));         \
    _PK(": TRATE(" #ts "), n=%d, t=%ld.%09ld, f=%1.3f Hz: " fmt, \
        n, _t.tv_sec, _t.tv_nsec, _f, ##args);                   \
  }

#define PKTRAW(ts)                _PKT("TRAW("  #ts ")", ts, "")
#define PKTRAWS(ts, str)          _PKT("TRAWS(" #ts ")", ts, ": %s", str)
#define PKTRAWF(ts, fmt, args...) _PKT("TRAWF(" #ts ")", ts, ": " fmt, ##args)

/* -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=-
 * HEX-DUMP MESSAGES:
 */

/* These macros are the intended public interface for producing hex-dumps, with
 * a header message, and should be used from within your application.
 *
 *   - PKDUMP()  - Print a hexdump with the given format string and associated
 *                 arguments as the header. The dump will be wrapped with cut
 *                 marks, and all output will be prefixed with the same file,
 *                 line number and function name. "DUMP" is present in the
 *                 generated output.
 *   - PKLINES() - Print a multi-line block of text with the given format string
 *                 and associated arguments as the header. The output will be
 *                 wrapped wit cut marks, and all output will be prefixed with
 *                 the same file, line number and function name. "LINES" is
 *                 present in the generated output. Output is terminated on a
 *                 nul character ('\0').
 */
#define PKDUMP(data, len, fmt, args...)             \
  {                                                 \
    PKF("DUMP: " fmt, ##args);                      \
    PKF("DUMP: %zu bytes @ %p", (size_t)len, data); \
    if ((data != NULL) && (len != 0)) {             \
      PKF("DUMP: ---8<---[ dump begins ]---8<---"); \
      _pk_dump(_PKFL, __func__, data, len);         \
      PKF("DUMP: ---8<---[  dump ends  ]---8<---"); \
    }                                               \
  }

#define PKLINES(data, len, fmt, args...)                                            \
  {                                                                                 \
    int i = 0; size_t ll; const char *ls, *p = NULL;                                \
    PKF("LINES: " fmt, ##args);                                                     \
    PKF("LINES: %zu chars max @ %p", (size_t)len, data);                            \
    if ((data != NULL) && (len != 0)) {                                             \
      PKF("LINES: ---8<---[ output begins ]---8<---");                              \
      for (i = 0; (ls = _pk_nextchunk(data, len, &p, &ll, '\n')) != NULL; i += 1) { \
        PKF("LINES: %05d: %.*s", i, (int)ll, ls);                                   \
      }                                                                             \
      PKF("LINES: ---8<---[  output ends  ]---8<---");                              \
    }                                                                               \
  }

/* the following macros are copied from the uSHET project:
 *    https://github.com/18sg/uSHET/blob/master/lib/cpp_magic.h
 * please refer to the source for documentation */
#define EVAL(...) EVAL1024(__VA_ARGS__)
#define EVAL1024(...) EVAL512(EVAL512(__VA_ARGS__))
#define EVAL512(...) EVAL256(EVAL256(__VA_ARGS__))
#define EVAL256(...) EVAL128(EVAL128(__VA_ARGS__))
#define EVAL128(...) EVAL64(EVAL64(__VA_ARGS__))
#define EVAL64(...) EVAL32(EVAL32(__VA_ARGS__))
#define EVAL32(...) EVAL16(EVAL16(__VA_ARGS__))
#define EVAL16(...) EVAL8(EVAL8(__VA_ARGS__))
#define EVAL8(...) EVAL4(EVAL4(__VA_ARGS__))
#define EVAL4(...) EVAL2(EVAL2(__VA_ARGS__))
#define EVAL2(...) EVAL1(EVAL1(__VA_ARGS__))
#define EVAL1(...) __VA_ARGS__
#define EMPTY()
#define COMMA() ,
#define BOOL(x) NOT(NOT(x))
#define DEFER2(id) id EMPTY EMPTY()()
#define CAT(a, ...) a ## __VA_ARGS__
#define FIRST(a, ...) a
#define SECOND(a, b, ...) b
#define IS_PROBE(...) SECOND(__VA_ARGS__, 0)
#define PROBE() ~, 1
#define NOT(x) IS_PROBE(CAT(_NOT_, x))
#define _NOT_0 PROBE()
#define IF(c) _IF(BOOL(c))
#define _IF(c) CAT(_IF_,c)
#define _IF_0(...)
#define _IF_1(...) __VA_ARGS__
#define HAS_ARGS(...) BOOL(FIRST(_END_OF_ARGUMENTS_ __VA_ARGS__)(0))
#define _END_OF_ARGUMENTS_(...) BOOL(FIRST(__VA_ARGS__))
#define MAP_PAIRS(op,sep,...) \
  IF(HAS_ARGS(__VA_ARGS__))(EVAL(MAP_PAIRS_INNER(op,sep,__VA_ARGS__)))
#define MAP_PAIRS_INNER(op,sep,cur_val_1, cur_val_2, ...) \
  op(cur_val_1,cur_val_2) \
  IF(HAS_ARGS(__VA_ARGS__))( \
    sep() DEFER2(_MAP_PAIRS_INNER)()(op, sep, __VA_ARGS__) \
  )
#define _MAP_PAIRS_INNER() MAP_PAIRS_INNER
#define MAP_PAIRS_ARG(op,sep,arg,...) \
  IF(HAS_ARGS(__VA_ARGS__))(EVAL(MAP_PAIRS_ARG_INNER(op,sep,arg,__VA_ARGS__)))
#define MAP_PAIRS_ARG_INNER(op,sep,arg,cur_val_1, cur_val_2, ...) \
  op(arg,cur_val_1,cur_val_2) \
  IF(HAS_ARGS(__VA_ARGS__))( \
    sep() DEFER2(_MAP_PAIRS_ARG_INNER)()(op, sep, arg, __VA_ARGS__) \
  )
#define _MAP_PAIRS_ARG_INNER() MAP_PAIRS_ARG_INNER

#endif /* PK_H */
