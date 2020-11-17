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
 */
#define _PK(fmt, args...) PK_FUNC(PK_TAG ": " _PKFL " %s()" fmt, __func__, ##args)

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

#endif /* PK_H */
