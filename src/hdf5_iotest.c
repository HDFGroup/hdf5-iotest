/* hdf5-iotest -- simple I/O performance tester for HDF5

   SPDX-License-Identifier: BSD-3-Clause

   Copyright (C) 2020, The HDF Group

   hdf5-iotest is released under the New BSD license (see COPYING).
   Go to the project home page for more info:

   https://github.com/HDFGroup/hdf5-iotest

*/

#include "dataset.h"
#include "write_test.h"
#include "read_test.h"

#include "hdf5.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define CONFIG_FILE "hdf5_iotest.ini"

typedef struct
{
  double min_create_time;
  double max_create_time;
  double min_write_phase;
  double max_write_phase;
  double min_write_time;
  double max_write_time;
  double min_read_phase;
  double max_read_phase;
  double min_read_time;
  double max_read_time;
} timings;

void print_config(const char* ini, configuration* pconfig);

void print_results
(
 configuration* pconfig,
 double         wall_time,
 hsize_t        fsize,
 timings*       pts
 );

herr_t set_libver_bounds(configuration* config, int rank, hid_t fapl);

int main(int argc, char* argv[])
{
  const char* ini = (argc > 1) ? argv[1] : CONFIG_FILE;

  configuration config;
  unsigned int strong_scaling_flg, coll_mpi_io_flg;

  int size, rank, my_proc_row, my_proc_col;
  unsigned long my_rows, my_cols;

  unsigned int irank, islow, ifill, ilay, ifmt, imod;

  char slow_dim[2][16] = { "step", "array" };
  char fill[2][16]     = { "true", "false" };
  char layout[2][16]   = { "contiguous", "chunked" };
  char fmt_low[2][16]  = { "earliest", "latest" };
  char mpi_mod[2][16]  = { "independent", "collective" };

  hid_t dxpl, fapl;
  hsize_t fsize;

  double wall_time, create_time, write_phase, write_time, read_phase, read_time;
  timings ts;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if (rank == 0) /* rank 0 reads and checks the config. file */
    {
      if (ini_parse(ini, handler, &config) < 0)
        {
          printf("Can't load '%s'\n", ini);
          return 1;
        }

      sanity_check(&config);
      validate(&config, size);
    }

  /* broadcast the input parameters */
  MPI_Bcast(&config, sizeof(configuration), MPI_BYTE, 0, MPI_COMM_WORLD);

  my_proc_row = rank / config.proc_cols;
  my_proc_col = rank % config.proc_cols;

  /* create the output CSV file */

  if (rank == 0)
    {
      FILE *fptr = fopen(config.csv_file, "w");
      assert(fptr != NULL);
      fprintf(fptr, "steps,arrays,rows,cols,scaling,proc-rows,proc-cols,"
              "slowdim,rank,alignment-increment,alignment-threshold,"
              "layout,fill,mpi-io,wall [s],fsize [B],"
              "write-phase-min [s],write-phase-max [s],"
              "creat-min [s],creat-max [s],"
              "write-min [s],write-max [s],"
              "read-phase-min [s],read-phase-max [s],"
              "read-min [s],read-max [s]\n");
      fclose(fptr);
    }

  /* TODO: initialize the configuration */

  strong_scaling_flg = (strncmp(config.scaling, "strong", 16) == 0);
  my_rows = strong_scaling_flg ? config.rows/config.proc_rows : config.rows;
  my_cols = strong_scaling_flg ? config.cols/config.proc_cols : config.cols;

  assert((dxpl = H5Pcreate(H5P_DATASET_XFER)) >= 0);

  assert((fapl = H5Pcreate(H5P_FILE_ACCESS)) >= 0);
  assert(H5Pset_fapl_mpio(fapl, MPI_COMM_WORLD, MPI_INFO_NULL) >= 0);
  if (config.alignment_increment > 1)
    assert(H5Pset_alignment(fapl, config.alignment_threshold,
                            config.alignment_increment) >= 0);

  for (irank = 2; irank <= 4; ++irank) {
    config.rank = irank;

    for (islow = 0; islow <= 1; ++islow) {
      strncpy(config.slowest_dimension, slow_dim[islow],
              sizeof(config.slowest_dimension));

      for (ilay = 0; ilay <= 1; ++ilay) {
        strncpy(config.layout, layout[ilay], sizeof(config.layout));

        for (ifill = 0; ifill <= 1; ++ifill) {
          strncpy(config.fill_values, fill[ifill], sizeof(config.fill_values));

          for (ifmt = 0; ifmt <= 1; ++ifmt) {
            strncpy(config.libver_bound_low, fmt_low[ifmt],
                    sizeof(config.libver_bound_low));

            assert(set_libver_bounds(&config, rank, fapl) >= 0);

            for (imod = 0; imod <= 1; ++imod) {
              strncpy(config.mpi_io, mpi_mod[imod], sizeof(config.mpi_io));

              coll_mpi_io_flg = (strncmp(config.mpi_io, "collective", 16) == 0);
              if (coll_mpi_io_flg)
                assert(H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_COLLECTIVE) >= 0);
              else
                assert(H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_INDEPENDENT) >= 0);

              if (rank == 0)
                print_config(ini, &config);

              MPI_Barrier(MPI_COMM_WORLD);

              wall_time = -MPI_Wtime();
              read_time = write_time = create_time = 0.0;

              write_phase = -MPI_Wtime();
              write_test(&config, size, rank, my_proc_row, my_proc_col,
                         my_rows, my_cols, fapl, dxpl,
                         &create_time, &write_time);
              write_phase += MPI_Wtime();

              MPI_Barrier(MPI_COMM_WORLD);

              read_phase = -MPI_Wtime();
              read_test(&config, size, rank, my_proc_row, my_proc_col,
                        my_rows, my_cols, fapl, dxpl, &read_time);
              read_phase += MPI_Wtime();

              MPI_Barrier(MPI_COMM_WORLD);

              wall_time += MPI_Wtime();

              if (rank == 0) /* retrieve the file size */
                {
                  hid_t file;
                  assert((file =
                          H5Fopen(config.hdf5_file, H5F_ACC_RDONLY, H5P_DEFAULT))
                         >= 0);
                  assert(H5Fget_filesize(file, &fsize) >= 0);
                  assert(H5Fclose(file) >= 0);
                }

              ts.max_write_phase = ts.min_write_phase = 0.0;
              ts.max_create_time = ts.min_create_time = 0.0;
              ts.max_write_time = ts.min_write_time = 0.0;
              ts.max_read_phase = ts.min_read_phase = 0.0;
              ts.max_read_time = ts.min_read_time = 0.0;

              MPI_Reduce(&write_phase, &ts.min_write_phase, 1, MPI_DOUBLE,
                         MPI_MIN, 0, MPI_COMM_WORLD);
              MPI_Reduce(&write_phase, &ts.max_write_phase, 1, MPI_DOUBLE,
                         MPI_MAX, 0, MPI_COMM_WORLD);
              MPI_Reduce(&create_time, &ts.min_create_time, 1, MPI_DOUBLE,
                         MPI_MIN, 0, MPI_COMM_WORLD);
              MPI_Reduce(&create_time, &ts.max_create_time, 1, MPI_DOUBLE,
                         MPI_MAX, 0, MPI_COMM_WORLD);
              MPI_Reduce(&write_time, &ts.min_write_time, 1, MPI_DOUBLE,
                         MPI_MIN, 0, MPI_COMM_WORLD);
              MPI_Reduce(&write_time, &ts.max_write_time, 1, MPI_DOUBLE,
                         MPI_MAX, 0, MPI_COMM_WORLD);
              MPI_Reduce(&read_phase, &ts.min_read_phase, 1, MPI_DOUBLE,
                         MPI_MIN, 0, MPI_COMM_WORLD);
              MPI_Reduce(&read_phase, &ts.max_read_phase, 1, MPI_DOUBLE,
                         MPI_MAX, 0, MPI_COMM_WORLD);
              MPI_Reduce(&read_time, &ts.min_read_time, 1, MPI_DOUBLE,
                         MPI_MIN, 0, MPI_COMM_WORLD);
              MPI_Reduce(&read_time, &ts.max_read_time, 1, MPI_DOUBLE,
                         MPI_MAX, 0, MPI_COMM_WORLD);

              if (rank == 0)
                print_results(&config, wall_time, fsize, &ts);
            }
          }
        }
      }
    }
  }

  assert(H5Pclose(fapl) >= 0);
  assert(H5Pclose(dxpl) >= 0);

  MPI_Finalize();

  return 0;
}

void print_results
(
 configuration* pconfig,
 double         wall_time,
 hsize_t        fsize,
 timings*       pts
 )
{
  FILE *fptr = fopen(pconfig->csv_file, "a");

  /* write results to console */
  printf("\nWall clock [s]:\t\t%.2f\n", wall_time);
  printf("File size [B]:\t\t%.0f\n", (double)fsize);
  printf("---------------------------------------------\n");
  printf("Measurement:\t\t_MIN (over MPI ranks)\n");
  printf("\t\t\t^MAX (over MPI ranks)\n");
  printf("---------------------------------------------\n");
  printf("Write phase [s]:\t_%.2f\n\t\t\t^%.2f\n", pts->min_write_phase,
         pts->max_write_phase);
  printf("Create time [s]:\t_%.2f\n\t\t\t^%.2f\n", pts->min_create_time,
         pts->max_create_time);
  printf("Write time [s]:\t\t_%.2f\n\t\t\t^%.2f\n", pts->min_write_time,
         pts->max_write_time);
  printf("Read phase [s]:\t\t_%.2f\n\t\t\t^%.2f\n", pts->min_read_phase,
         pts->max_read_phase);
  printf("Read time [s]:\t\t_%.2f\n\t\t\t^%.2f\n", pts->min_read_time,
         pts->max_read_time);

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
