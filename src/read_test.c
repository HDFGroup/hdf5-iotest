/* hdf5-iotest -- simple I/O performance tester for HDF5

   SPDX-License-Identifier: BSD-3-Clause

   Copyright (C) 2020, The HDF Group

   hdf5-iotest is released under the New BSD license (see COPYING).
   Go to the project home page for more info:

   https://github.com/HDFGroup/hdf5-iotest

*/

#include "read_test.h"

#include "dataset.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void read_test
(
 configuration* pconfig,
 char * hdf5_filename,
 int size,
 int rank,
 int my_proc_row,
 int my_proc_col,
 unsigned long my_rows,
 unsigned long my_cols,
 hid_t fapl,
 hid_t dapl,
 hid_t dxpl,
 double* create_time,
 double* read_time
 )
{
  unsigned int step_first_flg, strong_scaling_flg;
  unsigned int istep, iarray;
  double *rbuf;
  hid_t mspace;

  char path[255];

  hid_t file, dset, fspace;

  time_step *es = NULL;
  size_t    num_in_progress;
  hbool_t   op_failed;

#ifdef VERIFY_DATA
  /* Extent of the logical 4D array and partition origin/offset */
  size_t d[4], o[4];

  /*
   * The C-order of an index [i0, i1, i2, i3] in a 4D array of extent
   * [D0, D1, D2, D3] is ((i0*D1 + i1)*D2 + i2)*D3 + i3.
   *
   * The extent depends on the kind of scaling and, in parallel, we need
   * to adjust the indices by the partition origin/offset.
   */
#endif

  step_first_flg = (strncmp(pconfig->slowest_dimension, "step", 16) == 0);

  rbuf = (double*) calloc(my_rows*my_cols, sizeof(double));
  { /* create the in-memory dataspace */
    hsize_t dims[2];
    dims[0] = (hsize_t)my_rows;
    dims[1] = (hsize_t)my_cols;
    mspace = H5Screate_simple(2, dims, dims);
    assert(H5Sselect_all(mspace) >= 0);
  }

#ifdef VERIFY_DATA
  strong_scaling_flg = (strncmp(pconfig->scaling, "strong", 16) == 0);

  d[2] = strong_scaling_flg ? pconfig->rows : pconfig->rows * pconfig->proc_rows;
  d[3] = strong_scaling_flg ? pconfig->cols : pconfig->cols * pconfig->proc_cols;

  o[2] = strong_scaling_flg ? rank * my_rows : my_proc_row * pconfig->rows;
  o[3] = strong_scaling_flg ? rank * my_cols : my_proc_col * pconfig->cols;
  if (rank == 0)
    printf("\n\033[1;31m WARNING: Data verification enabled. Timings will be distorted!!!\033[0m\n");
#endif

#if H5_VERSION_GE(1,14,0)
  if (pconfig->async == 1) {
    es    = calloc(1, sizeof(time_step));
    es->es_data      = H5EScreate();
    es->es_meta_data = H5EScreate();
  }
#endif

#if H5_VERSION_GE(1,14,0)
  if(es != NULL)
    assert((file = H5Fopen_async(hdf5_filename, H5F_ACC_RDONLY, fapl, 0)) >= 0);
  else
#endif
    assert((file = H5Fopen(hdf5_filename, H5F_ACC_RDONLY, fapl)) >= 0);
  
  switch (pconfig->rank)
    {
    case 4:
      {
#if H5_VERSION_GE(1,14,0)
        if(es != NULL)
          assert((dset = H5Dopen_async(file, "dataset", dapl, es->es_meta_data)) >= 0);
        else
#endif
        assert((dset = H5Dopen(file, "dataset", dapl)) >= 0);

        for (istep = 0; istep < pconfig->steps; ++istep)
          {
            for (iarray = 0; iarray < pconfig->arrays; ++iarray)
              {
                assert((fspace = H5Dget_space(dset)) >= 0);
                *create_time -= MPI_Wtime();
                create_selection(pconfig, fspace, my_proc_row, my_proc_col,
                                 istep, iarray);
                *create_time += MPI_Wtime();
                *read_time -= MPI_Wtime();
#if H5_VERSION_GE(1,14,0)
                if(es != NULL)
                  assert(H5Dread_async(dset, H5T_NATIVE_DOUBLE, mspace, fspace, dxpl, rbuf, es->es_data) >= 0);
                else
#endif
                  assert(H5Dread(dset, H5T_NATIVE_DOUBLE, mspace, fspace, dxpl, rbuf) >= 0);
                *read_time += MPI_Wtime();
                assert(H5Sclose(fspace) >= 0);

#ifdef VERIFY_DATA
                d[0] = step_first_flg ? pconfig->steps : pconfig->arrays;
                d[1] = step_first_flg ? pconfig->arrays : pconfig->steps;
                o[0] = step_first_flg ? istep : iarray;
                o[1] = step_first_flg ? iarray : istep;
                verify_read_buffer(rbuf, &my_rows, &my_cols, d, o);
#endif
              }

            /* Simulate the compute phase */
            if (pconfig->delay.enable == 1) {
              if (istep != pconfig->steps - 1) { // no sleep after the last es
                if (rank == 0)
                  printf("Read Computing... \n");
                async_sleep(file, fapl, pconfig->delay);
              }
            }
            /* Even though we are reading the same data at each time step, normally we would need to 
             * fill the read buffer again before reading the next time step. Here we
             * make sure reading has completed before "filling" the read buffer again */
#if H5_VERSION_GE(1,14,0)
            if(es != NULL)
              H5ESwait(es->es_data, H5ES_WAIT_FOREVER, &num_in_progress, &op_failed);
#endif

          }
#if H5_VERSION_GE(1,14,0)
        if(es != NULL)
          assert(H5Dclose_async(dset, es->es_meta_data) >= 0);
        else
#endif
          assert(H5Dclose(dset) >= 0);
      }

      break;
    case 3:
      {
        if (step_first_flg) /* dataset per step */
          {
            for (istep = 0; istep < pconfig->steps; ++istep)
              {
                sprintf(path, "step=%d", istep);
                assert((dset = H5Dopen(file, path, dapl)) >= 0);
                assert((fspace = H5Dget_space(dset)) >= 0);

                for (iarray = 0; iarray < pconfig->arrays; ++iarray)
                  {
                    *create_time -= MPI_Wtime();
                    create_selection(pconfig, fspace, my_proc_row,
                                     my_proc_col, istep, iarray);
                    *create_time += MPI_Wtime();

                    *read_time -= MPI_Wtime();
#if H5_VERSION_GE(1,14,0)
                    if(es != NULL)
                      assert(H5Dread_async(dset, H5T_NATIVE_DOUBLE, mspace, fspace, dxpl, rbuf, es->es_data) >= 0);
                    else
#endif
                      assert(H5Dread(dset, H5T_NATIVE_DOUBLE, mspace, fspace, dxpl, rbuf) >= 0);
                    *read_time += MPI_Wtime();

#ifdef VERIFY_DATA
                    d[0] = pconfig->steps; d[1] = pconfig->arrays;
                    o[0] = istep; o[1] = iarray;
                    verify_read_buffer(rbuf, &my_rows, &my_cols, d, o);
#endif
                  }

                assert(H5Sclose(fspace) >= 0);
#if H5_VERSION_GE(1,14,0)
                if(es != NULL)
                  assert(H5Dclose_async(dset, es->es_meta_data) >= 0);
                else
#endif
                  assert(H5Dclose(dset) >= 0);

                if (pconfig->delay.enable == 1) {
                  if (istep != pconfig->steps - 1) { // no sleep after the last es
                    if (rank == 0)
                      printf("Read Computing... \n");
                    async_sleep(file, fapl, pconfig->delay);
                  }
                }
                /* Even though we are reading the same data at each time step, normally we would need to 
                 * fill the read buffer again before reading the next time step. Here we
                 * make sure reading has completed before "filling" the read buffer again */
#if H5_VERSION_GE(1,14,0)
                if(es != NULL)
                  H5ESwait(es->es_data, H5ES_WAIT_FOREVER, &num_in_progress, &op_failed);
#endif
              }
          }
        else /* dataset per array */
          {
            for (istep = 0; istep < pconfig->steps; ++istep)
              {
                for (iarray = 0; iarray < pconfig->arrays; ++iarray)
                  {
                    sprintf(path, "array=%d", iarray);
                    assert((dset = H5Dopen(file, path, dapl)) >= 0);
                    assert((fspace = H5Dget_space(dset)) >= 0);
                    *create_time -= MPI_Wtime();
                    create_selection(pconfig, fspace, my_proc_row,
                                     my_proc_col, istep, iarray);
                    *create_time += MPI_Wtime();

                    *read_time -= MPI_Wtime();
#if H5_VERSION_GE(1,14,0)
                    if(es != NULL)
                      assert(H5Dread_async(dset, H5T_NATIVE_DOUBLE, mspace, fspace, dxpl, rbuf, es->es_data) >= 0);
                    else
#endif
                      assert(H5Dread(dset, H5T_NATIVE_DOUBLE, mspace, fspace, dxpl, rbuf) >= 0);
                    *read_time += MPI_Wtime();

                    assert(H5Sclose(fspace) >= 0);
#if H5_VERSION_GE(1,14,0)
                    if(es != NULL)
                      assert(H5Dclose_async(dset, es->es_meta_data) >= 0);
                    else
#endif
                      assert(H5Dclose(dset) >= 0);

#ifdef VERIFY_DATA
                    d[0] = pconfig->arrays; d[1] = pconfig->steps;
                    o[0] = iarray; o[1] = istep;
                    verify_read_buffer(rbuf, &my_rows, &my_cols, d, o);
#endif
                  }

                if (pconfig->delay.enable == 1) {
                  if (istep != pconfig->steps - 1) { // no sleep after the last es
                    if (rank == 0)
                      printf("Read Computing... \n");
                    async_sleep(file, fapl, pconfig->delay);
                  }
                }
                /* Even though we are reading the same data at each time step, normally we would need to 
                 * fill the read buffer again before reading the next time step. Here we
                 * make sure reading has completed before "filling" the read buffer again */
#if H5_VERSION_GE(1,14,0)
                if(es != NULL)
                  H5ESwait(es->es_data, H5ES_WAIT_FOREVER, &num_in_progress, &op_failed);
#endif
              }
          }
      }
      break;
    case 2:
      {
        for (istep = 0; istep < pconfig->steps; ++istep)
          {
            for (iarray = 0; iarray < pconfig->arrays; ++iarray)
              {
                /* group per step or array */
                sprintf(path, (step_first_flg ?
                               "step=%d/array=%d" : "array=%d/step=%d"),
                        (step_first_flg ? istep : iarray),
                        (step_first_flg ? iarray : istep));

                assert((dset = H5Dopen(file, path, dapl)) >= 0);

                assert((fspace = H5Dget_space(dset)) >= 0);
                *create_time -= MPI_Wtime();
                create_selection(pconfig, fspace, my_proc_row, my_proc_col,
                                 istep, iarray);
                *create_time += MPI_Wtime();

                *read_time -= MPI_Wtime();
#if H5_VERSION_GE(1,14,0)
                if(es != NULL)
                  assert(H5Dread_async(dset, H5T_NATIVE_DOUBLE, mspace, fspace, dxpl, rbuf, es->es_data) >= 0);
                else
#endif
                  assert(H5Dread(dset, H5T_NATIVE_DOUBLE, mspace, fspace, dxpl, rbuf) >= 0);
                *read_time += MPI_Wtime();

                assert(H5Sclose(fspace) >= 0);
#if H5_VERSION_GE(1,14,0)
                if(es != NULL)
                  assert(H5Dclose_async(dset, es->es_meta_data) >= 0);
                else
#endif
                  assert(H5Dclose(dset) >= 0);

#ifdef VERIFY_DATA
                d[0] = step_first_flg ? pconfig->steps : pconfig->arrays;
                d[1] = step_first_flg ? pconfig->arrays : pconfig->steps;
                o[0] = step_first_flg ? istep : iarray;
                o[1] = step_first_flg ? iarray : istep;
                verify_read_buffer(rbuf, &my_rows, &my_cols, d, o);
#endif
              }

            if (pconfig->delay.enable == 1) {
              if (istep != pconfig->steps - 1) { // no sleep after the last es
                if (rank == 0)
                  printf("Read Computing... \n");
                async_sleep(file, fapl, pconfig->delay);
              }
            }
            /* Even though we are reading the same data at each time step, normally we would need to 
             * fill the read buffer again before reading the next time step. Here we
             * make sure reading has completed before "filling" the read buffer again */
#if H5_VERSION_GE(1,14,0)
            if(es != NULL)
              H5ESwait(es->es_data, H5ES_WAIT_FOREVER, &num_in_progress, &op_failed);
#endif
          }
      }
      break;
    default:
      break;
    }

#if H5_VERSION_GE(1,14,0)
  if(es != NULL) {
    if (pconfig->async == 1) {
      H5ESwait(es->es_meta_data, H5ES_WAIT_FOREVER, &num_in_progress, &op_failed);
      H5ESclose(es->es_meta_data);
      H5ESclose(es->es_data);
    }
    assert(H5Fclose_async(file, 0) >= 0);
    free(es);
  } else
#endif
    assert(H5Fclose(file) >= 0);

  assert(H5Sclose(mspace) >= 0);
  free(rbuf);
}
