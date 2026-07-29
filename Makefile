CXX      := g++
CXXFLAGS := -std=c++23 -Wall -Wextra -g -Iinclude
TARGET   := lantalk
SRCS     := $(wildcard src/*.cpp)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f $(TARGET)