CC = gcc
CXX = g++
CFLAGS = -O2 -g0 -Wall -Werror
CXXFLAGS =
MACOS = -mmacosx-version-min=10.9

PREFIX = i686-w64-mingw32
WIN_CC = $(PREFIX)-gcc
WIN_CXX = $(PREFIX)-g++

LX_PREFIX = x86_64-linux-musl
LX_CC = $(LX_PREFIX)-gcc
LX_CXX = $(LX_PREFIX)-g++

all: confgen confgen.exe confgen-linux

confgen: confgen.cxx.o macserial.c.o tinyxml2.cxx.o
	$(CXX) $^ -s $(MACOS) -o $@ -framework IOKit -framework CoreFoundation

%.c.o: %.c
	$(CC) $(CFLAGS) $(MACOS) -c $^ -o $@

%.cxx.o: %.cpp
	$(CXX) $(CFLAGS) $(CXXFLAGS) $(MACOS) -c $^ -o $@

confgen-linux: confgen.linux.o macserial.linuxc.o arc4random.linuxc.o tinyxml2.linux.o
	$(LX_CXX) $^ -s -static -o $@

%.linuxc.o: %.c
	$(LX_CC) $(CFLAGS) -c $^ -o $@

%.linux.o: %.cpp
	$(LX_CXX) $(CFLAGS) $(CXXFLAGS) -c $^ -o $@

confgen.exe: confgen.win.o macserial.winc.o arc4random.winc.o tinyxml2.win.o
	$(WIN_CXX) $^ -s -o $@ -lole32 -static

%.winc.o: %.c
	$(WIN_CC) $(CFLAGS) -c $^ -o $@

%.win.o: %.cpp
	$(WIN_CXX) $(CFLAGS) $(CXXFLAGS) -c $^ -o $@

install: all
	cp confgen confgen.exe confgen-linux ../bin/

clean:
	rm -f *.o
	rm -f confgen confgen.exe confgen-linux

.PHONY: clean all install
