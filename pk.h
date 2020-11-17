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
#   define PK_FUNC(fmt, args...) fprintf(stderr,   fmt "\n", ##args)
# endif
#endif

/* Optionally define PK_TAG to label the messages. Every message will contain
 * this text to support better filtering of any messages generated.
 */
#ifndef PK_TAG
# define PK_TAG "ATTIE"
#endif

/* -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=- -=#=-
 * INTERNAL / SUPPORT:
 */

/* These macros are used to build static strings containing the filename and
 * line number. _PKFL expands to this string, _PKS and _PKS2 should not be used.
 */
#define _PKS2(x) #x
#define _PKS(x) _PKS2(x)
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
#define _PKT(tag, ts, fmt, args...) _PK(": " tag " @ %ld.%09ld: " fmt, ts.tv_sec, ts.tv_nsec, ##args);

/* These macros take care of time-based calculations. They can be used from user
 * code if necessary.
 *
 *   - _PKTDIFF() - Put the difference between start and "now" into the diff
 *                  argument.
 *   - _PKTACC()  - Accumulate the difference between start and "now" into the
 *                  acc argument.
 */
#define _PKTDIFF(start, diff)                                   \
  {                                                             \
    PKTSTART(diff);                                             \
    if (diff.tv_nsec < start.tv_nsec)                           \
      { diff.tv_sec -= 1; diff.tv_nsec += 1000000000; }         \
    diff.tv_sec -= start.tv_sec; diff.tv_nsec -= start.tv_nsec; \
  }

#define _PKTACC(start, acc)                             \
  {                                                     \
    struct timespec _t; PKTSTART(_t);                   \
    _PKTDIFF(start, _t);                                \
    acc.tv_nsec += _t.tv_nsec; acc.tv_sec += _t.tv_sec; \
    if (acc.tv_nsec >= 1000000000)                      \
      { acc.tv_sec += 1; acc.tv_nsec -= 1000000000; }   \
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
 */
#define PK()               _PK("")
#define PKS(str)           _PK(": %s", str)
#define PKF(fmt, args...)  _PK(": " fmt, ##args)
#define PKV(fmt, var)      _PK(": " #var ": " fmt, var)
#define PKVB(fmt, var)     _PK(": " #var ": [" fmt "]", var)

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
 */
#ifdef __KERNEL__
# define PKTSTART(ts) getrawmonotonic(&ts)
#else
# include <time.h>
# define PKTSTART(ts)                                                \
  {                                                                  \
    int _ret;                                                        \
    if ((_ret = clock_gettime(CLOCK_REALTIME, &ts)) != 0)            \
      PKF("PKTSTART: clock_gettime(&" #ts ") returned %d...", _ret); \
  }
#endif

#define PKTSTAMP(fmt, args...)        \
  {                                   \
    struct timespec _t; PKTSTART(_t); \
    _PKT("TSTAMP", _t, fmt, ##args);  \
  }

#define PKTDIFF(ts, fmt, args...)            \
  {                                          \
    struct timespec _t; _PKTDIFF(ts, _t);    \
    _PKT("TDIFF(" #ts ")", _t, fmt, ##args); \
  }

#define PKTACC(ts, acc, fmt, args...)         \
  {                                           \
    _PKTACC(ts, acc);                         \
    _PKT("TACC(" #acc ")", acc, fmt, ##args); \
  }

#endif /* PK_H */
