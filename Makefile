CFLAGS ?= -DNDEBUG -Wall -Wextra -pedantic -std=c99 -g -fcolor-diagnostics -fansi-escape-codes
CXXFLAGS ?= -DNDEBUG -Wall -Wextra -pedantic -std=c++20 -g -fcolor-diagnostics -fansi-escape-codes
LDLIBS = -lm
SOURCES_C := renderer.c microui.c
SOURCES_CXX := main.cc
OBJECTS := $(SOURCES_C:%.c=%.o) $(SOURCES_CXX:%.cc=%.o)
DEPS := $(SOURCES_C:%.c=%.d) $(SOURCES_CXX:%.cc=%.d)
TARGET = native
MAIN = main

ifeq ($(OS),Windows_NT)
    MAIN = main.exe
    LDLIBS += -lgdi32
else ifeq ($(TARGET), mingw)
    MAIN = main.exe
    export CC = x86_64-w64-mingw32-gcc
    LDLIBS += -lgdi32
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        LDLIBS += -framework Cocoa
    else
        LDLIBS += -lX11
    endif
endif

$(MAIN): $(OBJECTS)
	$(CXX) -o $(MAIN) $(OBJECTS) $(LDLIBS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

-include $(DEPS)

clean:
	rm -f $(MAIN) $(OBJECTS) $(DEPS)
.PHONY: clean