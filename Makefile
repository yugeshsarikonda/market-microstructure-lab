CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS += -Iinclude
BUILD ?= build

.PHONY: all test sanitize clean
all: $(BUILD)/microbook-replay

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/microbook-replay: src/replay.cpp include/microbook/book.hpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/replay.cpp -o $@

$(BUILD)/book-tests: tests/book_tests.cpp include/microbook/book.hpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/book_tests.cpp -o $@

test: $(BUILD)/book-tests $(BUILD)/microbook-replay
	./$(BUILD)/book-tests
	python3 tests/test_replay.py ./$(BUILD)/microbook-replay

sanitize:
	$(MAKE) BUILD=build-sanitize CXXFLAGS="-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer" test

clean:
	rm -rf build build-sanitize
