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

void print_results
(
 configuration* pconfig,
 double         wall_time,
 timings*       pts
 )
{
  FILE *fptr = fopen(pconfig->csv_file, "a");
  hid_t file;
  hsize_t fsize;

  assert((file = H5Fopen(pconfig->hdf5_file, H5F_ACC_RDONLY, H5P_DEFAULT)) >= 0);
  assert(H5Fget_filesize(file, &fsize) >= 0);
  assert(H5Fclose(file) >= 0);

  /* write summary to the console */
  printf("\nWall clock [s]:\t\t%.2f\n", wall_time);
  printf("File size [B]:\t\t%.0f\n", (double)fsize);
  printf("---------------------------------------------\n");

  /* write results to the CSV file */
  assert(fptr != NULL);
  fprintf(fptr, "%d,%d,%ld,%ld,%s,%d,%d,%s,%d,%ld,%ld,%s,%s,%s,"
          "%.2f,%.0f,%.2f,%.2f,%.2f,%.2f,"
          "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
          pconfig->steps, pconfig->arrays, pconfig->rows, pconfig->cols,
          pconfig->scaling, pconfig->proc_rows, pconfig->proc_cols,
          pconfig->slowest_dimension, pconfig->rank,
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

void print_config(const char* ini, configuration* pconfig)
{
  printf("Config loaded from '%s':\n\tsteps=%d, arrays=%d,"
         "rows=%ld, columns=%ld, scaling=%s\n",
         ini, pconfig->steps, pconfig->arrays, pconfig->rows, pconfig->cols,
         pconfig->scaling
         );
  printf("\tproc-grid=%dx%d, slowest-dimension=%s, rank=%d\n",
         pconfig->proc_rows, pconfig->proc_cols, pconfig->slowest_dimension,
         pconfig->rank);
  printf("\talignment-increment=%ld, alignment-threshold=%ld\n",
         pconfig->alignment_increment, pconfig->alignment_threshold);
  printf("\tlayout=%s, fill=%s, mpi-io=%s\n",
         pconfig->layout, pconfig->fill_values, pconfig->mpi_io);
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

  if (rank == 0)
    printf("\nHDF5 library version %d.%d.%d[low=%d,high=%d]\n",
           majnum, minnum, relnum, low, high);

  return result;
}
