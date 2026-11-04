CXX ?= g++
CXXFLAGS ?= -O3 -std=c++17 -fstack-protector-strong -D_FORTIFY_SOURCE=2 -Wall -Wextra
LDFLAGS ?= -pthread

SRCS = src/main.cpp \
       src/core/file_tree.cpp \
       src/core/fast_scanner.cpp \
       src/core/mft_reader.cpp \
       src/core/treemap.cpp \
       src/core/security.cpp \
       src/mcp/token_optimizer.cpp \
       src/mcp/mcp_protocol.cpp

TARGET = treemcp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean

