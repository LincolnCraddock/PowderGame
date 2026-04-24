CFLAGS ?= -DNDEBUG -Wall -Wextra -pedantic -std=c99 -ggdb
CXXFLAGS ?= -DNDEBUG -Wall -Wextra -pedantic -std=c++20 -ggdb
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
#allows for checking type of input gpu
ifeq ($(GPU_TYPE),CUDA)
    $(info cuda)
    CUDAFLAGS = -O3 -std=c++20 -Xptxas -O3 -Xcompiler -Wall,-Werror
    
    GPU.o: ProcessCuda.cu
		nvcc $(CUDAFLAGS) -c $< -o $@
    objects += GPU.o
    
else ifeq ($(GPU_TYPE),HIP)
    $(info hip)
    HIPFLAGS = --offload-arch=native
    #GPU.o: ProcessHip.cc
else ifeq ($(GPU_TYPE),METAL)
    $(info metal)
    test
#else
#    $(info 4)
endif

$(MAIN): $(OBJECTS)
	$(CXX) -o $(MAIN) $(OBJECTS) $(LDLIBS)
%.o: %.cc
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<
%.o: %.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<
-include $(DEPS)

.PHONY: clean cuda hip metal
clean:
	rm -f $(MAIN) $(OBJECTS) $(DEPS) GPU.o

cuda:
	make GPU_TYPE=CUDA
hip:
	make GPU_TYPE=HIP
metal:
	make GPU_TYPE=METAL
# define test
# 	MAIN = main
# endef
# test:
# 	$(call test)
# test1 = 3