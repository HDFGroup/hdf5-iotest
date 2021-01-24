/* hdf5-iotest -- simple I/O performance tester for HDF5

   SPDX-License-Identifier: BSD-3-Clause

   Copyright (C) 2020, The HDF Group

   hdf5-iotest is released under the New BSD license (see COPYING).
   Go to the project home page for more info:

   https://github.com/HDFGroup/hdf5-iotest

*/

#include "utils.h"
#include "read_test.h"
#include "write_test.h"

#include "hdf5.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define CONFIG_FILE "hdf5_iotest.ini"

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

  double wall_time, create_time, write_phase, write_time, read_phase, read_time;
  timings ts;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if (rank == 0) /* rank 0 reads and checks the config. file */
    {
      /* sensible defaults */
      config.rank = 4;

      if (ini_parse(ini, handler, &config) < 0)
        {
          printf("Can't load '%s'\n", ini);
          return 1;
        }
    }

  /* broadcast the input parameters */
  MPI_Bcast(&config, sizeof(configuration), MPI_BYTE, 0, MPI_COMM_WORLD);

  validate(&config, size);

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

  strong_scaling_flg = (strncmp(config.scaling, "strong", 16) == 0);
  my_rows = strong_scaling_flg ? config.rows/config.proc_rows : config.rows;
  my_cols = strong_scaling_flg ? config.cols/config.proc_cols : config.cols;

  assert((dxpl = H5Pcreate(H5P_DATASET_XFER)) >= 0);

  assert((fapl = H5Pcreate(H5P_FILE_ACCESS)) >= 0);
  assert(H5Pset_fapl_mpio(fapl, MPI_COMM_WORLD, MPI_INFO_NULL) >= 0);
  if (config.alignment_increment > 1)
    assert(H5Pset_alignment(fapl, config.alignment_threshold,
                            config.alignment_increment) >= 0);

  /* dataset rank */
  for (irank = 2; irank <= 4; ++irank) {
    config.rank = irank;

    /* slowest changing dimension */
    for (islow = 0; islow <= 1; ++islow) {
      strncpy(config.slowest_dimension, slow_dim[islow],
              sizeof(config.slowest_dimension));

      /* dataset layout */
      for (ilay = 0; ilay <= 1; ++ilay) {
        strncpy(config.layout, layout[ilay], sizeof(config.layout));

        /* write fill values */
        for (ifill = 0; ifill <= 1; ++ifill) {
          strncpy(config.fill_values, fill[ifill], sizeof(config.fill_values));

          /* lower libver bound */
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

              validate(&config, size);

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
                print_results(&config, wall_time, &ts);
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
