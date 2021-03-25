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

/* Optionally define PK_FUNC to redirect the message output. The arguments (fmt
 * and args...) are equivelant to those passed to printf().
 */
#ifndef PK_FUNC
# ifdef __KERNEL__
#   include <linux/printk.h>
#   define PK_FUNC(fmt, args...)  printk(KERN_INFO fmt,      ##args)
# else
#   include <stdio.h>
#   include <string.h>
#   define PK_FUNC(fmt, args...) fprintf(stderr,   fmt "\n", ##args)
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
# include <stdint.h>
# include <errno.h>
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
static inline void _pk_dump(const char *_pkfl, const char *_pkfn, void *_data, size_t len) {
	uint8_t *data = _data;
	size_t i, o;
	char buf_hex  [(PK_DUMP_WIDTH * 3) + 1];
	char buf_print[ PK_DUMP_WIDTH      + 1];

	for (i = 0; (data != NULL) && (i < len); i++) {
		o = i % PK_DUMP_WIDTH;

		snprintf(&(buf_hex[o * 3]), 4, " %02hhx", data[i]);
		buf_print[o] = ((data[i] >= ' ') && (data[i] <= '~')) ? data[i] : '.';

		if ((o < (PK_DUMP_WIDTH - 1)) && (i < (len - 1))) {
			continue;
		}

		PK_FUNC(PK_TAG ": %s %s(): DUMP: 0x%04zx:%-*.*s | %-*.*s",
			_pkfl, _pkfn, i - o,
			PK_DUMP_WIDTH * 3, (int)((o+1) * 3), buf_hex,
			PK_DUMP_WIDTH,     (int) (o+1),      buf_print
		);
	}
}

/* -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=-
 * GENERIC MESSAGES:
 */

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
 *                 string.
 *   - PKVB()    - Print the name and value of a variable using the given format
 *                 string. Additionally enclose the variable's value in square
 *                 brackets. This can be useful when printing strings that may
 *                 contain whitespace.
 *   - PKE()     - Print the given message, suffixed with the errno and relevant
 *                 string description - like perror().
 */
#define PK()               _PK("")
#define PKS(str)           _PK(": %s", str)
#define PKF(fmt, args...)  _PK(": " fmt, ##args)
#define PKV(fmt, var)      _PK(": " #var ": " fmt, var)
#define PKVB(fmt, var)     _PK(": " #var ": [" fmt "]", var)

#define PKE(fmt, args...)                                \
  {                                                      \
    int _e = errno;                                      \
    _PK(": " fmt ": %d / %s", ##args, _e, strerror(_e)); \
  }

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
 *                  vs. PKS().
 *   - PKTRAWF()  - The same as PKTRAW(), but with a format string - see PK()
 *                  vs. PKF().
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
#define PKTRAWS(ts, str)          _PKT("TRAWF(" #ts ")", ts, ": %s", str)
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

#endif /* PK_H */
