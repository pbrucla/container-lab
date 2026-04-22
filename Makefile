CFLAGS := -Wall -Wextra -pedantic -std=c99

.PHONY: all clean

all: build/week-4-activity

build/%: %.c
	[ -d build ] || mkdir build
	$(CC) $(CFLAGS) -o $@ $<
	$(EXTRACMDS)

build/test-limits: CC := musl-gcc
build/test-limits: CFLAGS += -static -Oz -s
build/delegate-cgroup: EXTRACMDS = sudo chown root:root $@ && sudo chmod ug+s $@

clean:
	rm -r build
