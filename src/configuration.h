/* hdf5-iotest -- simple I/O performance tester for HDF5

   SPDX-License-Identifier: BSD-3-Clause

   Copyright (C) 2020, The HDF Group

   hdf5-iotest is released under the New BSD license (see COPYING).
   Go to the project home page for more info:

   https://github.com/HDFGroup/hdf5-iotest

*/

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "ini.h"

#include "hdf5.h"

#include <limits.h>

/* Configuration parameters */

typedef struct
{
  int           version;
  unsigned int  steps;
  unsigned int  arrays;
  unsigned long rows;
  unsigned long cols;
  unsigned int  proc_rows;
  unsigned int  proc_cols;
  char          scaling[16];
  unsigned int  rank;
  char          slowest_dimension[16];
  char          libver_bound_low[16];
  char          libver_bound_high[16];
  hsize_t       alignment_increment;
  hsize_t       alignment_threshold;
  hsize_t       meta_block_size;
  char          layout[16];
  char          fill_values[8];
  char          single_process[16];
  char          mpi_io[16];
  char          hdf5_file[PATH_MAX+1];
  char          csv_file[PATH_MAX+1];
  unsigned int  restart;
  unsigned int  split;
  unsigned int  one_case;
  unsigned int  HDF5perCase;
  char          compress_type[16];
  unsigned int  compress_par[2];
} configuration;

extern int handler(void* user,
                   const char* section,
                   const char* name,
                   const char* value);

extern int validate(configuration* user, const int size);

#endif
