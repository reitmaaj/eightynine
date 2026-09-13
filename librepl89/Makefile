CC := gcc
CFLAGS := -std=c89 -pedantic-errors -Wall -Wextra -Werror \
    -Wconversion -Wsign-conversion -Wstrict-prototypes \
    -Wmissing-prototypes -Wold-style-definition -Wundef -Wshadow -Wformat=2 \
    -Iinclude -Isrc -I../libu89/include -O0 -g
BUILD := build
U89 := ../libu89

SRC := src/repl89_mem.c src/repl89_buf.c src/repl89_edit.c src/repl89_hist.c src/repl89_render.c src/repl89_key.c src/repl89_paste.c src/repl89_tty.c src/repl89.c
OBJ := $(patsubst src/%.c,$(BUILD)/%.o,$(SRC))
TEST_SRC := test/test_main.c test/test_buf.c test/test_edit.c test/test_history.c test/vt.c test/test_render.c test/test_key.c test/test_paste.c test/test_tty.c test/test_session.c test/test_fault.c test/test_example.c
LDLIBS := -lutil

.PHONY: all librepl89 examples test clean

all: librepl89

librepl89: $(BUILD)/librepl89.a

$(BUILD)/librepl89.a: $(OBJ)
	@mkdir -p $(BUILD)
	ar rcs $@ $^

$(BUILD)/%.o: src/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/test: $(TEST_SRC) $(BUILD)/librepl89.a
	$(CC) $(CFLAGS) -o $@ $(TEST_SRC) $(BUILD)/librepl89.a $(U89)/build/libu89.a $(LDLIBS)

$(BUILD)/echo: examples/echo.c $(BUILD)/librepl89.a
	$(CC) $(CFLAGS) -o $@ $< $(BUILD)/librepl89.a $(U89)/build/libu89.a $(LDLIBS)

$(BUILD)/echo_events: examples/echo_events.c $(BUILD)/librepl89.a
	$(CC) $(CFLAGS) -o $@ $< $(BUILD)/librepl89.a $(U89)/build/libu89.a $(LDLIBS)

examples: $(BUILD)/echo $(BUILD)/echo_events

test: $(BUILD)/test $(BUILD)/echo $(BUILD)/echo_events
	$(BUILD)/test

clean:
	rm -rf $(BUILD)
