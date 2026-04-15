CFLAGS := -Wall -Wextra -pedantic -std=c99

.PHONY: all clean

all: build/week-3-activity

build/%: %.c
	[ -d build ] || mkdir build
	cc $(CFLAGS) -o $@ $<

clean:
	rm -r build
