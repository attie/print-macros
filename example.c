/* Step 1. Configure (optional) */
#define PK_TAG "PK-EXAMPLE"
#define PK_DUMP_WIDTH 16

/* Step 2. Include pk.h */
#include "pk.h"

/* unistd.h for sleeping */
#include <unistd.h>

int main(int argc, char *argv[]) {
	int i, o;
	char s[] = "  test string with some whitespace  ";
	struct timespec t, a;

	/* Step 3. Use it!
	 * All output will include the PK_TAG, the file, line number, and function
	 * name
	 */

	/* --- generic messages */

	/* PK() and PKS() are simple... a bare message, and a message with a static
	 * string. Note that the string passed to PKS() is not a format string,
	 * while the string passed to PKF() _is_ a format string, and thus care
	 * needs to be taken to escape format specifiers
	 */
	PK();
	PKS("test message");

	i = 42;

	/* PKF() behaves just like printf() / printk() ... you can pass a format
	 * paired with variadic arguments. Note that the first string passed to
	 * PKF() is a format string, and thus care needs to be taken to escape
	 * format specifiers
	 */
	PKF("I'm about to talk about 'i'");
	PKF("'i' has the value %d", i);

	/* PKV() will print the variable's name and value. You can use square
	 * brackets in the format string for values that may have whitespace
	 */
	PKV("%d", i);
	PKV("[%s]", s);

	/* PKV() may also be used to print multiple name / value pairs
	 */
	PKV("%d", i, "[%s]", s);

	/* PKE() will output the value of errno, and the relevant descripive string
	 */
	errno = EINVAL;
	PKE("uhoh");
	PKE("uhoh, myfunc() failed %d times", 3);

	/* --- time-based messages --- */

	/* PKTSTART() permits you to get the current time... effectively starting a
	 * timer. It doesn't produce any output
	 */
	PKTSTART(t);

	/* PKTSTAMP() permits a simple timestamp, with format string and arguments.
	 * The output is the Unix timestamp specified to nano seconds, but this may
	 * not be attainable on your system.
	 */
	PKTSTAMP("the answer is %d", i);

	/* PKTDIFF() outputs the difference in time between "now" and the reference
	 * timestamp
	 */
	PKTDIFF(t, "that was fast!");

	/* PKTACC() permits accumulation of time in a given variable. For example
	 * you may want to profile a certain portion of a loop, but not the entire
	 * loop. _PKTACC() may be used if you wish to accumulate without producing
	 * output
	 */
	memset(&a, 0, sizeof(a));
	for(o = 0; o < 5; o++) {
		/* this sleep is not counted */
		usleep(10000);

		PKTSTART(t);
		/* but this sleep is */
		usleep(1000);
		PKTACC(t, a, "iteration %d", o);
	}

	/* PKTRATE() permits automatic calculation of the frequency (n/t)
	 */
	PKTSTART(t);
	usleep(10000);
	PKTRATE(t, 10, "we waited for ~10ms for 10 items... which is ~1ms each, or 1 kHz!");

	/* PKTRAW(), PKTRAWS(), and PKTRAWF() permit printing a raw timestamp,
	 * perhaps as acquired by other means. These functinos are equivelant to
	 * PK(), PKS() and PKF() respectively. The same note applies re: care with
	 * format specifiers
	 */
	clock_gettime(CLOCK_REALTIME, &t);
	PKTRAW(t);
	PKTRAWS(t, "static message");
	PKTRAWF(t, "format string %d", i);

	/* --- hex-dump messages --- */

	/* PKDUMP() will produce a nice looking hex-dump. The dump's width can be
	 * adjusted by defining PK_DUMP_WIDTH to a value of your choosing. The
	 * pointer and length will be displayed above the dump. A NULL or length
	 * of zero still produce output. The dump can be gathered by the begin and
	 * end cut-marks, as well as the fact that all output has the same line
	 * number
	 */
	PKDUMP(NULL, 0, "this has no data or length");
	PKDUMP(s, sizeof(s), "this is our friendly string");

	/* PKLINES() will produce a nice looking multi-line block of text, and
	 * shares many characteristics with PKDUMP() */
	PKLINES("test block\n\nof\ntext", 128, "this is a multi-line string");
}
