CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude/handsdown
LDFLAGS ?=
LDLIBS ?=

BIN := handsdown
SRC := $(wildcard src/*.c src/input/*.c)
OBJ := $(patsubst src/%.c, build/%.o, $(SRC))
DEP := $(OBJ:.o=.d)

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(DEP)

clean:
	rm -rf build/$(BIN) $(OBJ) $(DEP) $(BIN)
