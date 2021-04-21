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
          "meta-block-size,layout,fill,fmt,io,wall [s],fsize [B],"
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
  hsize_t fsize,fsize_units;

  unsigned majnum, minnum, relnum;
  char version[16];
  assert(H5get_libversion(&majnum, &minnum, &relnum) >= 0);
  snprintf(version, 16, "\"%d.%d.%d\"", majnum, minnum, relnum);

  assert((file = H5Fopen(pconfig->hdf5_file, H5F_ACC_RDONLY, H5P_DEFAULT)) >= 0);
  assert(H5Fget_filesize(file, &fsize) >= 0);
  assert(H5Fclose(file) >= 0);

  /* write summary to the console */
  printf("Wall clock  [s]:\t\t%.2f\n", wall_time);

  static const char *UNIT[] = { "B", "kiB", "MiB", "GiB", "TiB", "PiB", "EiB" };
  hsize_t cnt = 0;
  hsize_t rem = 0;
  fsize_units=fsize;
  while (fsize_units >= 1024 && cnt < (sizeof UNIT / sizeof *UNIT)) {
    rem = (fsize_units % 1024);
    fsize_units /= 1024;
    cnt++;
  }
  printf("File size [%s]:\t\t%.1f\n", UNIT[cnt], (float)fsize_units + (float)rem / 1024.0);

  { /* write results to the CSV file */
    FILE *fptr = fopen(pconfig->csv_file, "a");
    assert(fptr != NULL);
    fprintf(fptr, "%d,%d,%ld,%ld,%s,%d,%d,%s,%d,%s,%llu,%llu,%llu,%s,%s,%s,%s,"
            "%.2f,%.0f,%.2f,%.2f,%.2f,%.2f,"
            "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
            pconfig->steps, pconfig->arrays, pconfig->rows, pconfig->cols,
            pconfig->scaling, pconfig->proc_rows, pconfig->proc_cols,
            pconfig->slowest_dimension, pconfig->rank, version,
            pconfig->alignment_increment, pconfig->alignment_threshold,
	    pconfig->meta_block_size,
            pconfig->layout, pconfig->fill_values, pconfig->libver_bound_low,
            pconfig->mpi_io, wall_time, (double)fsize,
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
  printf("Config loaded from '%s':\n  steps=%d, arrays=%d, "
         "rows=%ld, columns=%ld, proc-grid=%dx%d, scaling=%s\n",
         ini, pconfig->steps, pconfig->arrays, pconfig->rows, pconfig->cols,
         pconfig->proc_rows, pconfig->proc_cols, pconfig->scaling
         );
}

void print_current_config(configuration* pconfig)
{
  unsigned int size = pconfig->proc_rows*pconfig->proc_cols;
  char io[16];

  if (size > 1)
      strncpy(io, (strncmp(pconfig->mpi_io, "collective", 16) == 0) ?
              "mpi-io-col" : "mpi-io-ind", 16);
  else
    {
      if (strncmp(pconfig->single_process, "posix", 16) == 0)
        strncpy(io, "posix", 16);
      else if (strncmp(pconfig->single_process, "core", 16) == 0)
        strncpy(io, "core", 16);
      else if (strncmp(pconfig->single_process, "mpi-io-uni", 16) == 0)
        strncpy(io, "mpi-io-uni", 16);
      else
        strncpy(io, "ufo-io", 16);
    }

  printf(HLINE "\n");
  printf("%s rk=%d %s fill=%s align-[incr:thold]=[%llu:%llu] mblk=%llu fmt=%s io=%s\n",
         pconfig->slowest_dimension, pconfig->rank,
         strncmp(pconfig->layout, "contiguous", 16) == 0 ? "cont" : "chkd",
         pconfig->fill_values,
         pconfig->alignment_increment, pconfig->alignment_threshold,
	 pconfig->meta_block_size,
         pconfig->libver_bound_low, io);
}

void get_timings
(
 double   write_phase,
 double   create_time,
 double   write_time,
 double   read_phase,
 double   read_time,
 timings* pts
 )
{
  pts->max_write_phase = pts->min_write_phase = 0.0;
  pts->max_create_time = pts->min_create_time = 0.0;
  pts->max_write_time = pts->min_write_time = 0.0;
  pts->max_read_phase = pts->min_read_phase = 0.0;
  pts->max_read_time = pts->min_read_time = 0.0;

  MPI_Reduce(&write_phase, &pts->min_write_phase, 1, MPI_DOUBLE,
             MPI_MIN, 0, MPI_COMM_WORLD);
  MPI_Reduce(&write_phase, &pts->max_write_phase, 1, MPI_DOUBLE,
             MPI_MAX, 0, MPI_COMM_WORLD);
  MPI_Reduce(&create_time, &pts->min_create_time, 1, MPI_DOUBLE,
             MPI_MIN, 0, MPI_COMM_WORLD);
  MPI_Reduce(&create_time, &pts->max_create_time, 1, MPI_DOUBLE,
             MPI_MAX, 0, MPI_COMM_WORLD);
  MPI_Reduce(&write_time, &pts->min_write_time, 1, MPI_DOUBLE,
             MPI_MIN, 0, MPI_COMM_WORLD);
  MPI_Reduce(&write_time, &pts->max_write_time, 1, MPI_DOUBLE,
             MPI_MAX, 0, MPI_COMM_WORLD);
  MPI_Reduce(&read_phase, &pts->min_read_phase, 1, MPI_DOUBLE,
             MPI_MIN, 0, MPI_COMM_WORLD);
  MPI_Reduce(&read_phase, &pts->max_read_phase, 1, MPI_DOUBLE,
             MPI_MAX, 0, MPI_COMM_WORLD);
  MPI_Reduce(&read_time, &pts->min_read_time, 1, MPI_DOUBLE,
             MPI_MIN, 0, MPI_COMM_WORLD);
  MPI_Reduce(&read_time, &pts->max_read_time, 1, MPI_DOUBLE,
             MPI_MAX, 0, MPI_COMM_WORLD);
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
