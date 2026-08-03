CXX      := g++
CXXFLAGS := -std=c++23 -Wall -Wextra -g -Iinclude
LDFLAGS  := -lcrypto
TARGET   := lantalk
SRCS     := $(wildcard src/*.cpp)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(TARGET)