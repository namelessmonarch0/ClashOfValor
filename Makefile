# Clash of Valor
#
# Sources are scoped to src/ rather than globbed across the repo root. A bare
# *.cpp glob once pulled a scratch file with its own main() into the game build
# and broke linking with "duplicate symbol '_main'". tests/ now carries the only
# other main(), so keeping the globs directory-scoped still matters.

CXX      := g++
CXXSTD   := -std=c++11
WARN     := -Wall -Wextra -Wimplicit-fallthrough
CXXFLAGS := $(CXXSTD) $(WARN) -g -Isrc
LDLIBS   := -lsqlite3

TARGET   := game
BUILDDIR := build

SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp,$(BUILDDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# Everything except main.o, so the test binary can supply its own entry point.
LIBOBJS := $(filter-out $(BUILDDIR)/main.o,$(OBJS))

TEST_SRC := $(wildcard tests/*.cpp)
TEST_BIN := $(BUILDDIR)/test_game

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LDLIBS) -o $@

# Objects and dependency files live under build/ instead of beside the sources,
# which keeps src/ readable and makes `clean` a single rm -rf.
$(BUILDDIR)/%.o: src/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

run: $(TARGET)
	./$(TARGET)

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(LIBOBJS) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(TEST_SRC) $(LIBOBJS) $(LDLIBS) -o $@

clean:
	rm -rf $(BUILDDIR) $(TARGET)

-include $(DEPS)
