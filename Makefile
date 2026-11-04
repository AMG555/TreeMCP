CXX ?= g++
CXXFLAGS ?= -O3 -std=c++17 -flto -s -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fstack-protector-strong -D_FORTIFY_SOURCE=2 -Wl,--gc-sections -Wall -Wextra
LDFLAGS ?= -pthread

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share

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

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -d $(DESTDIR)$(DATADIR)/applications
	install -m 644 treemcp.desktop $(DESTDIR)$(DATADIR)/applications/treemcp.desktop
	install -d $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps
	install -m 644 assets/logo.svg $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps/treemcp.svg

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(DATADIR)/applications/treemcp.desktop
	rm -f $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps/treemcp.svg

clean:
	rm -f $(TARGET)

.PHONY: all clean install uninstall
