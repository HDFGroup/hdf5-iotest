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

#define  TEST_FOR(INI, CHK, CNT) for ((INI); (CHK); (CNT)) {
#define  END_TEST }

#define CONFIG_FILE "hdf5_iotest.ini"

int main(int argc, char* argv[])
{
  const char* ini = (argc > 1) ? argv[1] : CONFIG_FILE;

  configuration config;
  unsigned int strong_scaling_flg, coll_mpi_io_flg;

  int size, rank, my_proc_row, my_proc_col;
  unsigned long my_rows, my_cols;

  unsigned int irank, islow, ifill, ilay, ialig, imblk, ifmt, imod;

  char* slow_dim[2]      = { "step", "array" };
  char* fill[2]          = { "true", "false" };
  char* layout[2]        = { "contiguous", "chunked" };
  hsize_t align_incr[2]  = { 1, 1 };
  hsize_t align_thold[2] = { 0, 0 };
  hsize_t mblk_size[2]   = { 2048, 0 };
  char* fmt_low[2]       = { "earliest", "latest" };
  char* mpi_mod[2]       = { "independent", "collective" };

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

  if (rank == 0)
    print_initial_config(ini, &config);

  my_proc_row = rank / config.proc_cols;
  my_proc_col = rank % config.proc_cols;

  /* create the output CSV file */
  if (rank == 0)
    create_output_file(config.csv_file);

  strong_scaling_flg = (strncmp(config.scaling, "strong", 16) == 0);
  my_rows = strong_scaling_flg ? config.rows/config.proc_rows : config.rows;
  my_cols = strong_scaling_flg ? config.cols/config.proc_cols : config.cols;

  assert((dxpl = H5Pcreate(H5P_DATASET_XFER)) >= 0);
  assert((fapl = H5Pcreate(H5P_FILE_ACCESS)) >= 0);

  if (size > 1 || (strncmp(config.single_process, "mpi-io-uni", 16) == 0))
    assert(H5Pset_fapl_mpio(fapl, MPI_COMM_WORLD, MPI_INFO_NULL) >= 0);
  else
    if (strncmp(config.single_process, "core", 16) == 0)
      H5Pset_fapl_core(fapl, 67108864, 1); /* 64 MB increments */

  /* use a macro to stop the indentation madness */

  /* ======================================================================== */
  /* dataset rank */
  TEST_FOR (irank = 2, irank <= 4, ++irank);
  config.rank = irank;

  /* ======================================================================== */
  /* slowest changing dimension */
  TEST_FOR (islow = 0, islow <= 1, ++islow);
  strncpy(config.slowest_dimension, slow_dim[islow],
          sizeof(config.slowest_dimension));

  /* ======================================================================== */
  /* dataset layout */
  TEST_FOR (ilay = 0, ilay <= 1, ++ilay);
  strncpy(config.layout, layout[ilay], sizeof(config.layout));

  /* ======================================================================== */
  /* write fill values */
  TEST_FOR (ifill = 0, ifill <= 1, ++ifill);
  strncpy(config.fill_values, fill[ifill], sizeof(config.fill_values));

  /* ======================================================================== */
  /* alignment */
  TEST_FOR (ialig = 0, ialig <= 1, ++ialig);
  if (ialig == 0) /* run the baseline first */
    {
      align_incr[1]  = config.alignment_increment;
      align_thold[1] = config.alignment_threshold;
      config.alignment_increment = align_incr[0];
      config.alignment_threshold = align_thold[0];
      assert(H5Pset_alignment(fapl, config.alignment_threshold,
			      config.alignment_increment) >= 0);
    }
  else
    { /* check if we need to run anything beyond the baseline */
      if (align_incr[1] == 1 && align_thold[1] == 0)
        continue;
      else
        {
          config.alignment_increment = align_incr[1];
          config.alignment_threshold = align_thold[1];
        }
    }

  assert(H5Pset_alignment(fapl, config.alignment_threshold,
			  config.alignment_increment) >= 0);

  /* ======================================================================== */
  /* meta block size */
  TEST_FOR (imblk = 0, imblk <= 1, ++imblk);
  if (imblk == 0) /* run the baseline first */
    {
      mblk_size[1]  = config.meta_block_size;
      config.meta_block_size = mblk_size[0];
    }
  else
    { /* check if we need to run anything beyond the baseline */
      if (mblk_size[1] == 2048)
        continue;
      else
        {
          config.meta_block_size = mblk_size[1];
        }
    }

  assert(H5Pset_meta_block_size(fapl, config.meta_block_size) >= 0);

  /* ======================================================================== */
  /* lower libver bound */
  TEST_FOR (ifmt = 0, ifmt <= 1, ++ifmt);
  strncpy(config.libver_bound_low, fmt_low[ifmt],
          sizeof(config.libver_bound_low));
  assert(set_libver_bounds(&config, rank, fapl) >= 0);

  /* ======================================================================== */
  /* MPI-IO mode */
  TEST_FOR (imod = 0, imod <= 1, ++imod);
  if (size > 1)
    {
      strncpy(config.mpi_io, mpi_mod[imod], sizeof(config.mpi_io)-1);
      coll_mpi_io_flg = (strncmp(config.mpi_io, "collective", 15) == 0);
      if (coll_mpi_io_flg)
        assert(H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_COLLECTIVE) >= 0);
      else
        assert(H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_INDEPENDENT) >= 0);
    }
  else
    {
      strncpy(config.mpi_io, config.single_process, sizeof(config.mpi_io));
      if (imod == 1)
        continue;
    }

  /* ######################################################################## */

  validate(&config, size);

  if (rank == 0)
    print_current_config(&config);

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
            my_rows, my_cols, fapl, dxpl, &create_time, &read_time);
  read_phase += MPI_Wtime();

  MPI_Barrier(MPI_COMM_WORLD);

  wall_time += MPI_Wtime();

  get_timings(write_phase, create_time, write_time, read_phase, read_time, &ts);

  if (rank == 0)
    print_results(&config, wall_time, &ts);

  /* ######################################################################## */

  END_TEST /* MPI-IO mode */
  END_TEST /* libver bound */
  END_TEST /* alignment */
  END_TEST /* meta block size */
  END_TEST /* fill */
  END_TEST /* layout */
  END_TEST /* slow dim. */
  END_TEST /* rank */

  assert(H5Pclose(fapl) >= 0);
  assert(H5Pclose(dxpl) >= 0);

  MPI_Finalize();

  return 0;
}
