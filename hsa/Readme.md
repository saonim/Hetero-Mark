Build
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

Execution 
===================
To execute any benchmark, simply run the executable in the benchmark's 
directory. Take iir_cl12 as an example, run the following command

```
cd iir_cl12
./IIR12
```

For command of each benchmark, use --help option of each executable.
