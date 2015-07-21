Build instruction
====================

Run the following instruction to compile the program. Make sure your current
directory is in Hetero-Mark/hsa

```
cmake .
make
```

Before executing a benchmark, kernels of the benchmark also need to be
compiled. To compile kernels, use the follow command in each benchmarks 
directory

```
cloc.sh kernels.cl
```
