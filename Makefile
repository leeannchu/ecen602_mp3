CXX      = g++
CXXFLAGS = -Wall -Wextra -g
TARGET   = server

all: $(TARGET)

$(TARGET): server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -rf $(TARGET) *.o *.dSYM

.PHONY: all clean
