.PHONY:=clean
.DEFAULT_GOAL=all

clean:
	rm -f example

run: all
	./example

all: example

example: example.c pk.h
	$(CC) -Wall $< -o $@
