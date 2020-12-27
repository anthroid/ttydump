.DEFAULT_GOAL := all

builddir = bin
srcdir = src
src = $(wildcard $(srcdir)/*.c)
bin = $(builddir)/$(notdir $(realpath .))

CC := gcc
LDFLAGS = 
CFLAGS = -Wall -c
OBJECTS = $(src:%.c=$(builddir)/%.o)

print:
	@echo 'builddir = $(builddir)'
	@echo 'srcdir = $(srcdir)'
	@echo 'src = $(src)'
	@echo 'CFLAGS = $(CFLAGS)'
	@echo 'LDFLAGS = $(LDFLAGS)'
	@echo 'OBJECTS = $(OBJECTS)'

all: $(bin)

$(builddir)/%.o: %.c
	-mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $<

$(bin): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

debug: CFLAGS += -DDEBUG -O0 -g3
debug: all

clean:
	rm -rf $(builddir)

.PHONY: all clean print
