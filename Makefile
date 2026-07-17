CXX	:= g++
CXXFLAGS := -std=c++14 -Wall -Wextra -g
TARGET	:= lantalk
SRCS	:= $(wildcard src/*.cpp)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f $(TARGET)
