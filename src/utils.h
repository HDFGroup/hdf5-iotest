/* hdf5-iotest -- simple I/O performance tester for HDF5

   SPDX-License-Identifier: BSD-3-Clause

   Copyright (C) 2020, The HDF Group

   hdf5-iotest is released under the New BSD license (see COPYING).
   Go to the project home page for more info:

   https://github.com/HDFGroup/hdf5-iotest

*/

#ifndef UTILS_H
#define UTILS_H

#include "configuration.h"

#include "hdf5.h"

typedef struct
{
  double min_write_phase;
  double max_write_phase;
  double min_create_time;
  double max_create_time;
  double min_write_time;
  double max_write_time;
  double min_read_phase;
  double max_read_phase;
  double min_read_time;
  double max_read_time;
} timings;

typedef struct
{
  unsigned int irank;
  unsigned int islow;
  unsigned int ifill;
  unsigned int ilay;
  unsigned int ialig;
  unsigned int imblk;
  unsigned int ifmt;
  unsigned int imod;
} restart_t;

void create_output_file(const char* fname);

void print_initial_config(const char* ini, configuration* pconfig);

void print_current_config(configuration* pconfig);

void get_timings
(
 double   write_phase,
 double   create_time,
 double   write_time,
 double   read_phase,
 double   read_time,
 timings* pts
 );

void print_results
(
 configuration* pconfig, 
 char*          hdf5_filename,
 double         wall_time,
 timings*       pts
 );

herr_t set_libver_bounds(configuration* config, int rank, hid_t fapl);

void restart(
             restart_t *ckpt, 
             const char* fname,
             char* slow_dim[],
             char* fill[],
             char* layout[],
             char* fmt_low[],
             char* mpi_mod[],
             hsize_t mblk_size[],
             hsize_t align_incr[]
);

#endif
