# Options to pass to the C compiler
CFLAGS = -std=c99 -O2 -Wall -pedantic -g

# List of programs to build
TARGETS = smallest_largest how_old

# Default rule, built if you just say "make"
all: $(TARGETS)

# Rules to build programs:

smallest_largest: smallest_largest.c
	$(CC) $(CFLAGS) -o $@ $<

how_old: how_old.c
	$(CC) $(CFLAGS) -o $@ $<

# We could instead have a single pattern rule for both programs:
# %: %.c
#	$(CC) $(CFLAGS) -o $@ $<

# Remove built programs and temporary files
clean:
	rm -f $(TARGETS) *~
