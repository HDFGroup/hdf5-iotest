# hdf5-iotest
This is a simple I/O performance tester for HDF5. It's purpose is to assess the
*performance variability* of a set of logically equivalent HDF5 representations
of a common pattern. The test repeatedly writes a set of 2D array variables
over a set of "time steps." An INI file (see below) controls how the array
variables are represented in HDF5 and certain other low-level aspects of HDF5
library behavior. (See also [ADIOS IOTEST](https://github.com/ornladios/ADIOS2/tree/master/source/utils/adios_iotest).)

Run with:
```
hdf5_iotest <INI file>
```

The INI file syntax is shown below:
```
[DEFAULT]
version = 0                ; Let's keep an open mind!
steps = 20                 ; Number of steps
arrays = 500               ; Number of 2D array variables
rows = 100                 ; Total number of array rows for strong scaling
                           ; Number of array rows per block for weak scaling
columns = 200              ; Total number of array columns for strong scaling
                           ; Number of array columns per block for weak scaling
process-rows = 1           ; Number of MPI-process rows:
                           ; rows % proc-rows == 0 for strong scaling
process-columns = 1        ; Number of MPI-process columns:
                           ; columns % proc-columns == 0 for strong scaling
scaling = weak             ; Scaling ([weak, strong])
dataset-rank = 4           ; Rank of the dataset(s) in the file ([2, 3, 4])
slowest-dimension = step   ; Slowest changing dimension ([step, array])
layout = contiguous        ; HDF5 dataset layout ([contiguous, chunked])
fill-chunks = true         ; Initialize the chunks with fill values ([true false])
mpi-io = independent       ; MPI I/O mode ([independent, collective])
hdf5-file = hdf5_iotest.h5 ; HDF5 output file name
csv-file = hdf5_iotest.csv ; CSV results file name
```

Rather than modifying the `[DEFAULT]` section, we recommend that testers create
a new section, e.g., `[CUSTOM]`, and overwrite the default values as needed.
For a basic set of 24 parameter combinations see the `combinator.sh` INI file
generator. Running:
```
combinator.sh <prows> <pcols>
```
will create 24 INI file for a `prows x pcols` topology of MPI processes.
