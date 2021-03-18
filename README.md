# Attie's `printf()` / `printk()` Support Macros

These are intended only for development purposes, and are not considered suitable for use in production-ready code.
This header file should perform correctly when used from both within the Linux kernel, as well as userspace.

Effort has been made to reduce the processing required at run-time, in an attempt at reducing unwanted side effects.

## Usage

See [`example.c`](./example.c) for example usage, and [`example.log`](./example.log) for the expected output.

The example application can be build and run by running `make run`.

## Configuration

- `PK_FUNC(fmt, args...)` - Provides a usable printf-like function that can be called to produce output.
  - Userspace default: `fprintf(stderr, fmt "\n", ##args)`
  - Kernel default: `printk(KERN_INFO fmt, ##args)`
- `PK_TAG` - Resolve to a string literal, that will be at the beginning of every output line.
  - Default: `"ATTIE"`
- `PK_DUMP_WIDTH` - Resolves to an integer literal, that will alter the width of the hex dump output.
  - Default: `16`

## Functions

### General

- `PK()` - A quick and easy notice that the execution path traversed this line.
- `PKS(str)` - The same as `PK()`, but includes a static string.
- `PKF(fmt, args...)` - The same as `PK()`, but permits a fully-formed format string and associated arguments.
- `PKV(fmt, var)` - Generates output with the variable's name and value, formatted by `fmt`.
- `PKVB(fmt, var)` - The same as `PKV()`, but with square braces around the value.
- `PKE(fmt, args...)` - The same as `PKF()`, but with the value of errno and relevant string description.

### Timing

- `PKTSTART(ts)` - Take the current system time, effectively starting a timer.
- `PKTSTAMP(fmt, args...)` - Generate output with the current system time.
- `PKTDIFF(ts, fmt, args...)` - Output the time since `ts` was captured by `PKTSTART()`.
- `PKTACC(ts, acc, fmt, args...)` - Accumulate the time since `ts` was captured into `acc`, and output the running total.
- `PKTRATE(ts, n, fmt, args...)` - Calculate the event rate, based on the time since `ts` was captured, and `n` items processed.
- `PKTRAW(ts)` - Just print the given timestamp, perhaps as acquired by other means.
- `PKTRAWS(ts, str)`, `PKTRAWF(ts, fmt, args...)` - Equivelant to `PKS()` and `PKF()` respectively.

### Hex Dump

- `PKDUMP(data, len, fmt, args...)` - Output the given format string, followed by the memory size and location, and finally a hex dump of this memory.
