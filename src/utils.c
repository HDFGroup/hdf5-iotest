/* hdf5-iotest -- simple I/O performance tester for HDF5

   SPDX-License-Identifier: BSD-3-Clause

   Copyright (C) 2020, The HDF Group

   hdf5-iotest is released under the New BSD license (see COPYING).
   Go to the project home page for more info:

   https://github.com/HDFGroup/hdf5-iotest

*/

#include "utils.h"

#include <assert.h>
#include <string.h>

#define HLINE "--------------------------------------------------------------"\
              "-----------------"

void create_output_file(const char* fname)
{
  FILE *fptr = fopen(fname, "w");
  assert(fptr != NULL);
  fprintf(fptr, "steps,arrays,rows,cols,scaling,proc-rows,proc-cols,"
          "slowdim,rank,version,alignment-increment,alignment-threshold,"
          "layout,fill,mpi-io,wall [s],fsize [B],"
          "write-phase-min [s],write-phase-max [s],"
          "creat-min [s],creat-max [s],"
          "write-min [s],write-max [s],"
          "read-phase-min [s],read-phase-max [s],"
          "read-min [s],read-max [s]\n");
  fclose(fptr);
}

void print_results
(
 configuration* pconfig,
 double         wall_time,
 timings*       pts
 )
{
  hid_t file;
  hsize_t fsize;
  unsigned majnum, minnum, relnum;
  char version[16];
  assert(H5get_libversion(&majnum, &minnum, &relnum) >= 0);
  snprintf(version, 16, "\"%d.%d.%d\"", majnum, minnum, relnum);

  assert((file = H5Fopen(pconfig->hdf5_file, H5F_ACC_RDONLY, H5P_DEFAULT)) >= 0);
  assert(H5Fget_filesize(file, &fsize) >= 0);
  assert(H5Fclose(file) >= 0);

  /* write summary to the console */
  printf("Wall clock [s]:\t\t%.2f\n", wall_time);
  printf("File size [B]:\t\t%.0f\n", (double)fsize);

  { /* write results to the CSV file */
    FILE *fptr = fopen(pconfig->csv_file, "a");
    assert(fptr != NULL);
    fprintf(fptr, "%d,%d,%ld,%ld,%s,%d,%d,%s,%d,%s,%ld,%ld,%s,%s,%s,"
            "%.2f,%.0f,%.2f,%.2f,%.2f,%.2f,"
            "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
            pconfig->steps, pconfig->arrays, pconfig->rows, pconfig->cols,
            pconfig->scaling, pconfig->proc_rows, pconfig->proc_cols,
            pconfig->slowest_dimension, pconfig->rank, version,
            pconfig->alignment_increment, pconfig->alignment_threshold,
            pconfig->layout,
            pconfig->fill_values, pconfig->mpi_io, wall_time, (double)fsize,
            pts->min_write_phase, pts->max_write_phase,
            pts->min_create_time, pts->max_create_time,
            pts->min_write_time, pts->max_write_time,
            pts->min_read_phase, pts->max_read_phase,
            pts->min_read_time, pts->max_read_time);
    fclose(fptr);
  }
}

void print_initial_config(const char* ini, configuration* pconfig)
{
  printf("Config loaded from '%s':\n  steps=%d, arrays=%d,"
         "rows=%ld, columns=%ld, proc-grid=%dx%d, scaling=%s\n",
         ini, pconfig->steps, pconfig->arrays, pconfig->rows, pconfig->cols,
         pconfig->proc_rows, pconfig->proc_cols, pconfig->scaling
         );
}

void print_current_config(configuration* pconfig)
{
  printf(HLINE "\n");
  printf("%s rk=%d %s fill=%s align-[incr:thold]=[%ld:%ld] mpi-io=%s\n",
         pconfig->slowest_dimension, pconfig->rank,
         strncmp(pconfig->layout, "contiguous", 16) == 0 ? "cont" : "chkd",
         pconfig->fill_values,
         pconfig->alignment_increment, pconfig->alignment_threshold,
         strncmp(pconfig->mpi_io, "collective", 16) == 0 ? "col" : "ind");
}


herr_t set_libver_bounds(configuration* pconfig, int rank, hid_t fapl)
{
  herr_t result = 0;
  H5F_libver_t low = H5F_LIBVER_EARLIEST, high = H5F_LIBVER_LATEST;
  unsigned majnum, minnum, relnum;
  assert((result = H5get_libversion(&majnum, &minnum, &relnum)) >= 0);
  assert (majnum == 1 && minnum >= 8 && minnum <= 13);

  if (strncmp(pconfig->libver_bound_low, "earliest", 16) != 0)
    {
      if (strncmp(pconfig->libver_bound_low, "v18", 16) == 0)
#if H5_VERSION_GE(1,10,0)
        low = H5F_LIBVER_V18;
#else
      low = H5F_LIBVER_LATEST;
#endif
      else if (strncmp(pconfig->libver_bound_low, "v110", 16) == 0)
#if H5_VERSION_GE(1,12,0)
        low = H5F_LIBVER_V110;
#else
      low = H5F_LIBVER_LATEST;
#endif
      else if (strncmp(pconfig->libver_bound_low, "v112", 16) == 0)
#if H5_VERSION_GE(1,13,0)
        low = H5F_LIBVER_V112;
#else
      low = H5F_LIBVER_LATEST;
#endif
      else
        low = H5F_LIBVER_LATEST;
    }

  if (strncmp(pconfig->libver_bound_high, "latest", 16) != 0)
    {
      if (strncmp(pconfig->libver_bound_high, "v18", 16) == 0)
#if H5_VERSION_GE(1,10,0)
        high = H5F_LIBVER_V18;
#else
      high = H5F_LIBVER_LATEST;
#endif
      else if (strncmp(pconfig->libver_bound_high, "v110", 16) == 0)
#if H5_VERSION_GE(1,12,0)
        high = H5F_LIBVER_V110;
#else
      high = H5F_LIBVER_LATEST;
#endif
      else if (strncmp(pconfig->libver_bound_high, "v112", 16) == 0)
#if H5_VERSION_GE(1,13,0)
        high = H5F_LIBVER_V112;
#else
      high = H5F_LIBVER_LATEST;
#endif
    }

  assert(low <= high);
  assert((result = H5Pset_libver_bounds(fapl, low, high)) >= 0);

  return result;
}