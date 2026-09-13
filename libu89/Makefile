CC := gcc
CFLAGS := -std=c89 -pedantic-errors -Wall -Wextra -Werror \
    -Wconversion -Wsign-conversion -Wstrict-prototypes \
    -Wmissing-prototypes -Wold-style-definition -Wundef -Wshadow -Wformat=2 \
    -Iinclude -Isrc -O0 -g
BUILD := build

SRC := src/u89_version.c src/u89_utf8.c src/u89_grapheme.c src/u89_xid.c src/u89_id.c src/u89_props.c src/u89_width.c src/u89_nfc.c src/u89_casefold.c src/u89_tables.c
OBJ := $(patsubst src/%.c,$(BUILD)/%.o,$(SRC))
TEST_SRC := test/test_main.c test/test_utf8.c test/test_grapheme.c test/test_xid.c test/test_id.c test/test_props.c test/test_casefold.c test/test_width.c test/test_nfc.c test/test_consistency.c

.PHONY: all libu89 test clean

all: libu89

libu89: $(BUILD)/libu89.a

$(BUILD)/libu89.a: $(OBJ)
	@mkdir -p $(BUILD)
	ar rcs $@ $^

$(BUILD)/%.o: src/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/test: $(TEST_SRC) $(BUILD)/libu89.a
	$(CC) $(CFLAGS) -o $@ $(TEST_SRC) $(BUILD)/libu89.a

test: $(BUILD)/test
	$(BUILD)/test

clean:
	rm -rf $(BUILD)
