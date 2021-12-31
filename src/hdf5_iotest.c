/* hdf5-iotest -- simple I/O performance tester for HDF5

   SPDX-License-Identifier: BSD-3-Clause

   Copyright (C) 2020, The HDF Group

   hdf5-iotest is released under the New BSD license (see COPYING).
   Go to the project home page for more info:

   https://github.com/HDFGroup/hdf5-iotest

*/

#include "read_test.h"
#include "utils.h"
#include "write_test.h"

#include "hdf5.h"

#include <uuid/uuid.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
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
  unsigned int ckpt_flg;
  restart_t ckpt;

  char* slow_dim[2]      = { "step", "array" };
  char* fill[2]          = { "true", "false" };
  char* layout[2]        = { "contiguous", "chunked" };
  hsize_t align_incr[2]  = { 1, 1 };
  hsize_t align_thold[2] = { 0, 0 };
  hsize_t mblk_size[2]   = { 2048, 0 };
  char* fmt_low[2]       = { "earliest", "latest" };
  char* mpi_mod[2]       = { "independent", "collective" };

  hid_t fcpl, fapl, dapl, dxpl, lcpl, fapl_cpy, fapl_split;

  double wall_time, create_time, write_phase, write_time, read_phase, read_time;
  timings ts;
  int icase = 0;
  int nmod = 0;

  int         mpi_thread_lvl_provided = -1;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &mpi_thread_lvl_provided);
  assert(MPI_THREAD_MULTIPLE == mpi_thread_lvl_provided);

  //MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  /* Turn off buffering of stdout */
  setbuf(stdout, NULL);

  if (rank == 0) /* rank 0 reads and checks the config. file */
    {
      uuid_t uuid;
      /* sensible defaults */
      config.rank = 4;
      config.hdf5_file[0] = '\0';
      config.csv_file[0] = '\0';
      config.restart = 0;
      config.split = 0;
      config.delay.time_num = 0;
      config.async = 0;
      config.one_case = 0;
      config.HDF5perCase = 0;
      config.compress_type[0] = '\0';

      if (ini_parse(ini, handler, &config) < 0)
        {
          printf("Can't load '%s'\n", ini);
          return 1;
        }
      if (config.csv_file[0] == '\0')
        {
          uuid_generate_time_safe(uuid);
          uuid_unparse_lower(uuid, config.csv_file);
          config.csv_file[36] = '.';
          config.csv_file[37] = 'c';
          config.csv_file[38] = 's';
          config.csv_file[39] = 'v';
          config.csv_file[40] = '\0';
        }

      printf("Output: %s\n", config.csv_file);
    }

  /* broadcast the input parameters */
  MPI_Bcast(&config, sizeof(configuration), MPI_BYTE, 0, MPI_COMM_WORLD);

  validate(&config, size);

  if (rank == 0)
    print_initial_config(ini, &config);

  ckpt_flg = 0;
  if (config.restart == 1) {
    if (rank == 0) /* rank 0 reads the last successful configuration */
      {
        restart(&ckpt, 
                config.csv_file,
                slow_dim,
                fill,
                layout,
                fmt_low,
                mpi_mod,
                mblk_size,
                align_incr);
      }
    /* broadcast the restart parameters */
    MPI_Bcast(&ckpt, sizeof(ckpt), MPI_BYTE, 0, MPI_COMM_WORLD);
    ckpt_flg = 1;
  }

  my_proc_row = rank / config.proc_cols;
  my_proc_col = rank % config.proc_cols;

  /* create the output CSV file */
  if (rank == 0 && config.restart == 0)
    create_output_file(config.csv_file);
  /* create the output checkpoint restart file */

  strong_scaling_flg = (strncmp(config.scaling, "strong", 16) == 0);
  my_rows = strong_scaling_flg ? config.rows/config.proc_rows : config.rows;
  my_cols = strong_scaling_flg ? config.cols/config.proc_cols : config.cols;

  assert((fcpl = H5Pcreate(H5P_FILE_CREATE)) >= 0);
  assert((fapl = H5Pcreate(H5P_FILE_ACCESS)) >= 0);
  assert((dapl = H5Pcreate(H5P_DATASET_ACCESS)) >= 0);
  assert((dxpl = H5Pcreate(H5P_DATASET_XFER)) >= 0);
  assert((lcpl = H5Pcreate(H5P_LINK_CREATE)) >= 0);
  assert(H5Pset_create_intermediate_group(lcpl, 1) >= 0);

  if (size > 1 || (strncmp(config.single_process, "mpi-io-uni", 16) == 0))
    {
      assert(H5Pset_fapl_mpio(fapl, MPI_COMM_WORLD, MPI_INFO_NULL) >= 0);
      assert(H5Pset_all_coll_metadata_ops(fapl, 1) >= 0);
      assert(H5Pset_coll_metadata_write(fapl, 1) >= 0);
    }
  else
    if (strncmp(config.single_process, "core", 16) == 0)
      assert(H5Pset_fapl_core(fapl, 67108864, 1) >= 0); /* 64 MB increments */
    else
      assert(H5Pset_fapl_sec2(fapl) >= 0);

  /* test collective and independent modes when parallel, and greater than 0 ranks */
  if (size > 1) nmod = 1;

  char hdf5_filename[strlen(config.hdf5_file+4)];

  /* use a macro to stop the indentation madness */

  /* ======================================================================== */
  /* dataset rank */
  TEST_FOR (irank = 2, irank <= 4, ++irank);
  if(config.restart == 1 && ckpt_flg == 1) irank = ckpt.irank;
  config.rank = irank;

  /* ======================================================================== */
  /* slowest changing dimension */
  TEST_FOR (islow = 0, islow <= 1, ++islow);
  if(config.restart == 1 && ckpt_flg == 1) islow= ckpt.islow;
  strncpy(config.slowest_dimension, slow_dim[islow],
          sizeof(config.slowest_dimension));

  /* ======================================================================== */
  /* dataset layout */
  TEST_FOR (ilay = 0, ilay <= 1, ++ilay);
  if(config.restart == 1 && ckpt_flg == 1) ilay = ckpt.ilay;
  strncpy(config.layout, layout[ilay], sizeof(config.layout));

  /* ======================================================================== */
  /* write fill values */
  TEST_FOR (ifill = 0, ifill <= 1, ++ifill);
  if(config.restart == 1 && ckpt_flg == 1) ifill = ckpt.ifill;
  strncpy(config.fill_values, fill[ifill], sizeof(config.fill_values));

  /* ======================================================================== */
  /* alignment */
  TEST_FOR (ialig = 0, ialig <= 1, ++ialig);
  if(config.restart == 1 && ckpt_flg == 1) {
    ialig = ckpt.ialig;
    if (ialig == 1) {
      align_incr[1]  = config.alignment_increment;
      align_thold[1] = config.alignment_threshold;
    }
  }
  if (ialig == 0) /* run the baseline first */
    {
      align_incr[1]  = config.alignment_increment;
      align_thold[1] = config.alignment_threshold;
      config.alignment_increment = align_incr[0];
      config.alignment_threshold = align_thold[0];
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
  if(config.restart == 1 && ckpt_flg == 1) {
    imblk = ckpt.imblk;
    if (imblk == 1) {
      mblk_size[1]  = config.meta_block_size;
    }
  }
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
          config.meta_block_size = mblk_size[1];
    }
  assert(H5Pset_meta_block_size(fapl, config.meta_block_size) >= 0);

  /* ======================================================================== */
  /* lower libver bound */
  TEST_FOR (ifmt = 0, ifmt <= 1, ++ifmt);
  if(config.restart == 1 && ckpt_flg == 1)  ifmt = ckpt.ifmt;
  strncpy(config.libver_bound_low, fmt_low[ifmt],
          sizeof(config.libver_bound_low));
  assert(set_libver_bounds(&config, rank, fapl) >= 0);

  /* ======================================================================== */
  /* MPI-IO mode */
  TEST_FOR (imod = 0, imod <= nmod, ++imod);
  ++icase;
  
  if(config.one_case > 0 && config.one_case != icase) goto skip;
  if(config.restart == 1 && ckpt_flg == 1) {
    imod = ckpt.imod;
    ckpt_flg = 0;
  }

  if (size > 1)
    {
      if(config.split == 0 )
        strncpy(config.mpi_io, mpi_mod[imod], sizeof(config.mpi_io)-1);
      else /* split driver can't do collective I/O */
        strncpy(config.mpi_io, mpi_mod[0], sizeof(config.mpi_io)-1);

      coll_mpi_io_flg = (strncmp(config.mpi_io, "collective", 15) == 0);

      if (coll_mpi_io_flg)
        assert(H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_COLLECTIVE) >= 0);
      else
        assert(H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_INDEPENDENT) >= 0);
    }
  else
    {
      if( strncmp(config.single_process, "mpi-io-uni", 16) == 0 &&
          strcmp(config.compress_type, "") != 0 ) {
        assert(H5Pset_dxpl_mpio(dxpl, H5FD_MPIO_COLLECTIVE) >= 0);
        coll_mpi_io_flg = 1;
      }
      strncpy(config.mpi_io, config.single_process, sizeof(config.mpi_io));
      if (imod == 1 && config.restart == 1 )
        continue;
    }

  /* Set the split file driver if requested.
   * This needs to be done here so that the metadata and raw data fapl's
   * parameters are completely set because some fapl APIs, like alignment,
   * can't be applied to the split driver's fapl */
  if(config.split == 1 ) 
    {
      assert((fapl_split = H5Pcreate(H5P_FILE_ACCESS)) >= 0);
      assert(H5Pset_fapl_split(fapl_split, "-m.h5", fapl, "-r.h5", fapl) >= 0);
      fapl_cpy = fapl;
      fapl = fapl_split;
    }

  /* ######################################################################## */

  validate(&config, size);

  if (rank == 0)
    print_current_config(&config);

  strcpy( hdf5_filename, config.hdf5_file);

  if(config.HDF5perCase != 0)
    {
      char buf[5];
      sprintf(buf, "%04d", icase);

      char * num;
      num = strstr (hdf5_filename,"#");
      strcpy (num+4, num+1);
      strncpy (num,buf,4);
    }

  MPI_Barrier(MPI_COMM_WORLD);

  wall_time = -MPI_Wtime();
  read_time = write_time = create_time = 0.0;

  write_phase = -MPI_Wtime();
  write_test(&config, hdf5_filename, size, rank, my_proc_row, my_proc_col, my_rows, my_cols,
             fcpl, fapl, lcpl, dapl, dxpl, coll_mpi_io_flg,
             &create_time, &write_time);
  write_phase += MPI_Wtime();

  MPI_Barrier(MPI_COMM_WORLD);

  read_phase = -MPI_Wtime();
  read_test(&config, hdf5_filename, size, rank, my_proc_row, my_proc_col, my_rows, my_cols,
            fapl, dapl, dxpl,
            &create_time, &read_time);

  read_phase += MPI_Wtime();

  MPI_Barrier(MPI_COMM_WORLD);

  wall_time += MPI_Wtime();

  get_timings(write_phase, create_time, write_time, read_phase, read_time, &ts);

  if (rank == 0)
    print_results(&config, hdf5_filename, wall_time, &ts);
  
  if (config.split == 1) 
    {
      assert(H5Pclose(fapl) >= 0); /* close the split driver fapl */
      fapl = fapl_cpy;
    }

  /* clean up the hdf5 files for the case of an HDF5 file per case */
  if(config.HDF5perCase != 0 && rank == 0) {
    int len = strlen(hdf5_filename) + 8;
    char* command = malloc( len );
    strcpy( command, "rm -f " );
    strcat( command,  hdf5_filename);
    system(command);
    free(command);
  }
  
  if(config.one_case > 0) goto exitloop;
 skip:
  continue;

  /* ######################################################################## */

  END_TEST /* MPI-IO mode */
  END_TEST /* libver bound */
  END_TEST /* meta block size */
  END_TEST /* alignment */
  END_TEST /* fill */
  END_TEST /* layout */
  END_TEST /* slow dim. */
  END_TEST /* rank */

 exitloop:

  assert(H5Pclose(lcpl) >= 0);
  assert(H5Pclose(dxpl) >= 0);
  assert(H5Pclose(dapl) >= 0);
  assert(H5Pclose(fapl) >= 0);
  assert(H5Pclose(fcpl) >= 0);

  MPI_Finalize();

  return 0;
}
