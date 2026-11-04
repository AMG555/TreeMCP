CXX ?= g++
CXXFLAGS ?= -O3 -std=c++17 -flto -s -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fstack-protector-strong -D_FORTIFY_SOURCE=2 -Wl,--gc-sections -Wall -Wextra
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
