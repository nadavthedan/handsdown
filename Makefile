CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic
CFLAGS += -pthread
CPPFLAGS += -Iinclude/handsdown $(shell pkg-config --cflags gtk+-3.0 gtk-layer-shell-0 libevdev)
LDFLAGS ?=
LDLIBS ?=
LDLIBS += $(shell pkg-config --libs gtk+-3.0 gtk-layer-shell-0 libevdev) -pthread -lm

BIN := handsdown_app
SRC := $(wildcard src/*.c src/input/*.c src/overlay/*.c src/modes/*.c)
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
