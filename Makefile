# Authors    : Lincoln Craddock, John Hershey
# Date       : 2026-04-27
# Description: complies main.cc, as well as renderer.c, renderer.h,
# microui.c, microui.h, and fenster.h into an executable, with the
# ability to compile against harware-specific files for parallelization,
# specifically, processCuda for Nvidia, processHip for AMD, 
# and processMetal for Apple
CFLAGS ?= -DNDEBUG -Wall -Wextra -pedantic -std=c99 -ggdb
CXXFLAGS ?= -DNDEBUG -Wall -Wextra -pedantic -std=c++20 -ggdb
LDLIBS = -lm
SOURCES_C := renderer.c microui.c
SOURCES_CXX := main.cc
OBJECTS := $(SOURCES_C:%.c=%.o) $(SOURCES_CXX:%.cc=%.o)
DEPS := $(SOURCES_C:%.c=%.d) $(SOURCES_CXX:%.cc=%.d)
TARGET = native
MAIN = main
#cxx for main, must be GPU-specific to link properly
MAINCXX := $(CXX)
#OS-specific compiliation
ifeq ($(OS),Windows_NT)
    MAIN := main.exe
    LDLIBS += -lgdi32
else ifeq ($(TARGET), mingw)
    MAIN := main.exe
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
#GPU-specific compilation
ifeq ($(GPU_TYPE),CUDA)
    $(info compiling for cuda)
    CUDAFLAGS = -O3 -std=c++20 -Xptxas -O3 -Xcompiler -Wall,-Werror
    OBJECTS += cudagpu.o
    MAINCXX := nvcc
    ifeq ($(MAIN), main.exe)
        MAIN:=mainCuda.exe
    else
        MAIN:=mainCuda
    endif
    all: cudagpu.o $(MAIN)
    
    cudagpu.o: processCuda.cu
		nvcc $(CUDAFLAGS) -c $< -o $@
else ifeq ($(GPU_TYPE),HIP)
    $(info hip)
    HIPFLAGS = --offload-arch=native -O3
    OBJECTS += hipgpu.o
    MAINCXX := hipcc
    ifeq ($(MAIN), main.exe)
        MAIN:=mainHip.exe
    else
        MAIN:=mainHip
    endif
    all: hipgpu.o $(MAIN)
    
    hipgpu.o: processHip.cc
		hipcc $(HIPFLAGS) -c $< -o $@
else ifeq ($(GPU_TYPE),METAL)
    $(info metal)
    METALFLAGS = -O3 #fix
    OBJECTS += metalgpu.o
    MAINCXX := metal #fix
    ifeq ($(MAIN), main.exe)
        MAIN:=mainMetal.exe
    else
        MAIN:=mainMetal
    endif
    all: metalgpu.o $(MAIN)
    
    metalgpu.o: processMetal.cc
		metal $(METALFLAGS) -c $< -o $@ #fix
else
    OBJECTS += processBase.o
endif

$(MAIN): $(OBJECTS)
	$(MAINCXX) -o $(MAIN) $(OBJECTS) $(LDLIBS)
%.o: %.cc
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<
%.o: %.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<
-include $(DEPS)

.PHONY: clean cuda hip metal
clean:
	rm -f $(MAIN) $(OBJECTS) $(DEPS) *.o

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