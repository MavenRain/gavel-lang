# Builds the gavel-lang kernel (the ratify-lang a46d5df and 71f2c22 kernel,
# a port of permit-lang 3d47120 and 18deea2) on the tcc-wasm host with
# TinyCC. See README.md. src/front/lower.c, src/wasm.c and src/ir.h are in
# the build from S3.
TCC ?= tcc
CC ?= cc
CFLAGS = -std=c99 -Wall -Werror -Isrc
CLANG_FLAGS = -std=c99 -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only -Isrc
FRONT = src/front/base.c src/front/lexer.c src/front/parser.c src/front/front.c src/front/eval.c src/front/check.c src/front/lower.c
TARGET = src/wasm.c
SRC = src/main.c $(FRONT) $(TARGET)
FRONT_TOOL = test/fronttool.c $(filter-out src/front/front.c src/front/lower.c,$(FRONT))
PATH_TOOL = test/pathtool.c $(filter-out src/front/front.c src/front/lower.c,$(FRONT))
NOTRAP_TOOL = test/notraptool.c
# The crypto layer (S8): the C oracle of test/cryptotool.c. langc does not
# link it; only build/cryptotool does.
CRYPTO = src/crypto/u256.c src/crypto/bn254.c src/crypto/keccak.c src/crypto/poseidon.c
CRYPTO_TOOL = test/cryptotool.c $(CRYPTO)
HEADERS = $(wildcard src/*.h src/front/*.h src/crypto/*.h)

.PHONY: build check check-clang clean

build: build/langc build/fronttool build/pathtool build/notraptool build/cryptotool

build/domain.c: domain/domain.lang gen/embed.c
	mkdir -p build
	$(TCC) -run gen/embed.c domain/domain.lang build/domain.c

build/langc: $(SRC) $(HEADERS) build/domain.c
	$(TCC) $(CFLAGS) -o build/langc $(SRC) build/domain.c

build/fronttool: $(FRONT_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/fronttool $(FRONT_TOOL)

build/pathtool: $(PATH_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/pathtool $(PATH_TOOL)

build/notraptool: $(NOTRAP_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/notraptool $(NOTRAP_TOOL)

build/cryptotool: $(CRYPTO_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/cryptotool $(CRYPTO_TOOL)

check-clang: build/domain.c
	$(CC) $(CLANG_FLAGS) $(SRC) build/domain.c
	$(CC) $(CLANG_FLAGS) $(FRONT_TOOL)
	$(CC) $(CLANG_FLAGS) $(PATH_TOOL)
	$(CC) $(CLANG_FLAGS) $(NOTRAP_TOOL)
	$(CC) $(CLANG_FLAGS) $(CRYPTO_TOOL)

check: build/langc build/fronttool build/pathtool build/notraptool build/cryptotool check-clang
	sh test/gate.sh
	sh test/review.sh
	build/fronttool
	build/pathtool

clean:
	rm -rf build
