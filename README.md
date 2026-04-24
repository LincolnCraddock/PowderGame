# PowderGame
Authors: Lincoln Craddock, John Hershey

Supporting Package: Fenster, https://github.com/zserge/fenster

## Installation
download the project
run "make"
run the executable

## Parallel versions
# CUDA (Nvidia GPUs)
install CUDA using the developer guide, if not installed
https://docs.nvidia.com/cuda/cuda-quick-start-guide/index.html
run "make cuda" to compile a cuda-capable executable
run mainCuda.*
# HIP (AMD GPUs)
install HIP using the developer guide, if not installed
https://rocm.docs.amd.com/projects/HIP/en/latest/install/install.html
run "make hip" to compile a cuda-capable executable
run mainHip.*
# Metal (Apple GPUs)
install Metal using the developer guide, if not installed

run "make metal" to compile a metal-capable executable
run mainMetal.*
## Backlog
- CUDA parallelization of processPowder() method
- Convert CUDA parallelization of processPowder() to HIP
- Metal parallelization of processPowder()
- Display instructions to exit (type ESC)
- Display instructions to reopen toolbar when closed (type CMD + T)
