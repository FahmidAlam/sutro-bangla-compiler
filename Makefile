# সূত্র / Shutro — build by Team Scorpion
#
#   make            build build/sutro.exe
#   make clean      remove build output
#   make test       run the test suite (tests/run_tests.py)
#
# CXX points at the MinGW g++ that is installed but not on PATH. Override it if
# your compiler lives somewhere else:  make CXX=g++

# ':=' not '?=' — make has a built-in default of CXX=g++ that '?=' would not
# replace, and a bare 'g++' cannot locate its own cc1plus unless MinGW is on PATH.
# A command-line override still wins:  make CXX=g++
CXX      := C:/MinGW/bin/g++.exe
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
PYTHON   ?= python

BUILD  := build
SRC    := $(wildcard src/*.cpp)
OBJ    := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRC))
TARGET := $(BUILD)/sutro.exe

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $@

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# A bare `mkdir` fails when the directory exists in both sh and cmd, and the
# leading '-' tells make to carry on. That keeps this Makefile portable across
# Git Bash and cmd without needing `mkdir -p`.
$(BUILD):
	-mkdir "$(BUILD)"

test: $(TARGET)
	$(PYTHON) tests/run_tests.py

# Two lines, both prefixed with '-' so a failure is ignored: the first works when
# make runs recipes through Git Bash, the second when it runs them through cmd.
clean:
	-rm -rf $(BUILD)
	-rmdir /s /q $(BUILD)
