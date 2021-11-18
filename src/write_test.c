/* hdf5-iotest -- simple I/O performance tester for HDF5

   SPDX-License-Identifier: BSD-3-Clause

   Copyright (C) 2020, The HDF Group

   hdf5-iotest is released under the New BSD license (see COPYING).
   Go to the project home page for more info:

   https://github.com/HDFGroup/hdf5-iotest

*/

#include "write_test.h"

#include "dataset.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void
sleep_(duration sleep_time)
{
    if (sleep_time.unit == TIME_SEC) {
        sleep(sleep_time.time_num);
    }
    else if (sleep_time.unit == TIME_MIN) {

        sleep(60 * sleep_time.time_num);
    }
    else {
        if (sleep_time.unit == TIME_MS)
            usleep(1000 * sleep_time.time_num);
        else if (sleep_time.unit == TIME_US)
            usleep(sleep_time.time_num);
        else
            printf("Invalid sleep time unit.\n");
    }
}

void
async_sleep(hid_t file_id, hid_t fapl, duration sleep_time)
{
#ifdef USE_ASYNC_VOL
    unsigned cap = 0;
    H5Pget_vol_cap_flags(fapl, &cap);
    if (H5VL_CAP_FLAG_ASYNC & cap)
        H5Fstart(file_id, fapl);
#endif
    sleep_(sleep_time);
}

void write_test
(
 configuration* pconfig,
 char * hdf5_filename,
 int size,
 int rank,
 int my_proc_row,
 int my_proc_col,
 unsigned long my_rows,
 unsigned long my_cols,
 hid_t fcpl,
 hid_t fapl,
 hid_t lcpl,
 hid_t dapl,
 hid_t dxpl,
 unsigned int coll_mpi_io_flg,
 double* create_time,
 double* write_time
 )
{
  unsigned int step_first_flg;
  unsigned int istep, iarray;
  double *wbuf;
  hid_t mspace;
  size_t i;

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

  wbuf = (double*) malloc(my_rows*my_cols*sizeof(double));
  { /* create the in-memory dataspace */
    hsize_t dims[2];
    dims[0] = (hsize_t)my_rows;
    dims[1] = (hsize_t)my_cols;
    mspace = H5Screate_simple(2, dims, dims);
    assert(H5Sselect_all(mspace) >= 0);
  }

#ifdef VERIFY_DATA
  unsigned int strong_scaling_flg;
  strong_scaling_flg = (strncmp(pconfig->scaling, "strong", 16) == 0);

  d[2] = strong_scaling_flg ? pconfig->rows : pconfig->rows * pconfig->proc_rows;
  d[3] = strong_scaling_flg ? pconfig->cols : pconfig->cols * pconfig->proc_cols;

  o[2] = strong_scaling_flg ? rank * my_rows : my_proc_row * pconfig->rows;
  o[3] = strong_scaling_flg ? rank * my_cols : my_proc_col * pconfig->cols;

  printf("\nWARNING: Data verification enabled. Timings will be distorted!!!\n");
#else

  /* add varability to data when compression is enabled */
  if (strncmp(pconfig->compress_type, "", 16) != 1) {
    float deltax, deltay;
    float x, y;
    size_t ii;
    size_t j;
    float x0 = 0.5f;
    float y0 = 0.5f;

    deltax = 1.f/( pconfig->rows-1);
    deltay = 1.f/( pconfig->cols-1);

    y = deltay;
    for(j = 0, ii = 0; j < (size_t)my_cols; j++) {
      x = deltax;
      for(i = 0; i < (size_t)my_rows; i++, ii++) {
        wbuf[ii] = (x-x0)*(x-x0) + (y-y0)*(y-y0);
        x += deltax;
      }
      y += deltay;
    }
  } else {
    for (i = 0; i < (size_t)my_rows*my_cols; ++i)
      wbuf[i] = (double) (my_proc_row + my_proc_col);
  }
#endif

  *create_time -= MPI_Wtime();
#if H5_VERSION_GE(1,13,0)
  if(es != NULL)
    assert((file = H5Fcreate_async(hdf5_filename, H5F_ACC_TRUNC, fcpl, fapl, 0)) >= 0);
  else
#endif
    assert((file = H5Fcreate(hdf5_filename, H5F_ACC_TRUNC, fcpl, fapl)) >= 0);

  *create_time += MPI_Wtime();

  //  int timestep_cnt = pconfig->steps;
  //for (int ts_index = 0; ts_index < timestep_cnt; ts_index++) {
  //  time_step *ts = &(time_steps[ts_index]);
  
#if H5_VERSION_GE(1,13,0)
  if (pconfig->async.enable == 1) {
    es    = calloc(1, sizeof(time_step));
    es->es_data      = H5EScreate();
    es->es_meta_data = H5EScreate();
  }
#endif

  switch (pconfig->rank)
    {
    case 4:
      {
        /* a single 4D array */
        *create_time -= MPI_Wtime();
        assert((dset = create_dataset(pconfig, file, "dataset", lcpl, dapl, coll_mpi_io_flg, es))
               >= 0);
        *create_time += MPI_Wtime();

        for (istep = 0; istep < pconfig->steps; ++istep)
          {
            for (iarray = 0; iarray < pconfig->arrays; ++iarray)
              {
#ifdef VERIFY_DATA
                d[0] = step_first_flg ? pconfig->steps : pconfig->arrays;
                d[1] = step_first_flg ? pconfig->arrays : pconfig->steps;
                o[0] = step_first_flg ? istep : iarray;
                o[1] = step_first_flg ? iarray : istep;
                init_write_buffer(wbuf, &my_rows, &my_cols, d, o);
#endif
                assert((fspace = H5Dget_space(dset)) >= 0);
                *create_time -= MPI_Wtime();
                create_selection(pconfig, fspace, my_proc_row, my_proc_col,
                                 istep, iarray);
                *create_time += MPI_Wtime();

                *write_time -= MPI_Wtime();
#if H5_VERSION_GE(1,13,0)
                if(es != NULL)
                  assert(H5Dwrite_async(dset, H5T_NATIVE_DOUBLE, mspace, fspace, dxpl, wbuf, es->es_data) >= 0);
                else
#endif
                  assert(H5Dwrite(dset, H5T_NATIVE_DOUBLE, mspace, fspace, dxpl, wbuf) >= 0);

                *write_time += MPI_Wtime();
                assert(H5Sclose(fspace) >= 0);
              }
            
            /* Simulate the compute phase */
            if (pconfig->async.enable == 1) {
              if (istep != pconfig->steps - 1) { // no sleep after the last es
                if (rank == 0)
                  printf("Write Computing... \n");
                async_sleep(file, fapl, pconfig->async);
              }

              /* Even though we are writing the same data at each time step, normally we would need to 
               * fill the write buffer again before outputting the next time step. Here we
               * make sure write has completed before "filling" the write buffer again */
#if H5_VERSION_GE(1,13,0)
              H5ESwait(es->es_data, H5ES_WAIT_FOREVER, &num_in_progress, &op_failed);
#endif
            }
 
          }
#if H5_VERSION_GE(1,13,0)
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
                *create_time -= MPI_Wtime();
                sprintf(path, "step=%d", istep);
                assert((dset = create_dataset(pconfig, file, path, lcpl, dapl, coll_mpi_io_flg, es))
                       >= 0);
                *create_time += MPI_Wtime();

                for (iarray = 0; iarray < pconfig->arrays; ++iarray)
                  {
#ifdef VERIFY_DATA
                    d[0] = pconfig->steps; d[1] = pconfig->arrays;
                    o[0] = istep; o[1] = iarray;
                    init_write_buffer(wbuf, &my_rows, &my_cols, d, o);
#endif
                    assert((fspace = H5Dget_space(dset)) >= 0);
                    *create_time -= MPI_Wtime();
                    create_selection(pconfig, fspace, my_proc_row,
                                     my_proc_col, istep, iarray);
                    *create_time += MPI_Wtime();

                    *write_time -= MPI_Wtime();
#if H5_VERSION_GE(1,13,0)
                    if(es != NULL)
                      assert(H5Dwrite_async(dset, H5T_NATIVE_DOUBLE, mspace, fspace, dxpl, wbuf, es->es_data) >= 0);
                    else
#endif
                      assert(H5Dwrite(dset, H5T_NATIVE_DOUBLE, mspace, fspace, dxpl, wbuf) >= 0);

                    *write_time += MPI_Wtime();
                    assert(H5Sclose(fspace) >= 0);
                  }
#if H5_VERSION_GE(1,13,0)
                if(es != NULL)
                  assert(H5Dclose_async(dset, es->es_meta_data) >= 0);
                else
#endif
                  assert(H5Dclose(dset) >= 0);

                if (pconfig->async.enable == 1) {
                  if (istep != pconfig->steps - 1) { // no sleep after the last es
                    if (rank == 0)
                      printf("Write Computing... \n");
                    async_sleep(file, fapl, pconfig->async);
                  }
                  
                  /* Even though we are writing the same data at each time step, normally we would need to 
                   * fill the write buffer again before outputting the next time step. Here we
                   * make sure write has completed before "filling" the write buffer again */
#if H5_VERSION_GE(1,13,0)
                  H5ESwait(es->es_data, H5ES_WAIT_FOREVER, &num_in_progress, &op_failed);
#endif
                }

              }
          }
        else /* dataset per array */
          {
            for (istep = 0; istep < pconfig->steps; ++istep)
              {
                for (iarray = 0; iarray < pconfig->arrays; ++iarray)
                  {
                    sprintf(path, "array=%d", iarray);
                    *create_time -= MPI_Wtime();
                    if (istep > 0)
                      assert((dset = H5Dopen(file, path, dapl)) >= 0);
                    else
                      assert((dset = create_dataset(pconfig, file, path,
                                                    lcpl, dapl, coll_mpi_io_flg, es)) >= 0);
                    *create_time += MPI_Wtime();

#ifdef VERIFY_DATA
                    d[0] = pconfig->arrays; d[1] = pconfig->steps;
                    o[0] = iarray; o[1] = istep;
                    init_write_buffer(wbuf, &my_rows, &my_cols, d, o);
#endif
                    assert((fspace = H5Dget_space(dset)) >= 0);
                    *create_time -= MPI_Wtime();
                    create_selection(pconfig, fspace, my_proc_row,
                                     my_proc_col, istep, iarray);
                    *create_time += MPI_Wtime();

                    *write_time -= MPI_Wtime();
#if H5_VERSION_GE(1,13,0)
                    if(es != NULL)
                      assert(H5Dwrite_async(dset, H5T_NATIVE_DOUBLE, mspace, fspace, dxpl, wbuf, es->es_data) >= 0);
                    else
#endif
                      assert(H5Dwrite(dset, H5T_NATIVE_DOUBLE, mspace, fspace, dxpl, wbuf) >= 0);

                    *write_time += MPI_Wtime();
                    assert(H5Sclose(fspace) >= 0);
#if H5_VERSION_GE(1,13,0)
                    if(es != NULL)
                      assert(H5Dclose_async(dset, es->es_meta_data) >= 0);
                    else
#endif
                      assert(H5Dclose(dset) >= 0); 
                  }

                if (pconfig->async.enable == 1) {
                  if (istep != pconfig->steps - 1) { // no sleep after the last es
                    if (rank == 0)
                      printf("Write Computing... \n");
                    async_sleep(file, fapl, pconfig->async);
                  }
                  /* Even though we are writing the same data at each time step, normally we would need to 
                   * fill the write buffer again before outputting the next time step. Here we
                   * make sure write has completed before "filling" the write buffer again */
#if H5_VERSION_GE(1,13,0)
                  H5ESwait(es->es_data, H5ES_WAIT_FOREVER, &num_in_progress, &op_failed);
#endif
                }

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
                *create_time -= MPI_Wtime();
                /* group per step or array of 2D datasets */
                sprintf(path, (step_first_flg ?
                               "step=%d/array=%d" : "array=%d/step=%d"),
                        (step_first_flg ? istep : iarray),
                        (step_first_flg ? iarray : istep));
                assert((dset = create_dataset(pconfig, file, path,
                                              lcpl, dapl, coll_mpi_io_flg, es)) >= 0);
                *create_time += MPI_Wtime();

#ifdef VERIFY_DATA
                d[0] = step_first_flg ? pconfig->steps : pconfig->arrays;
                d[1] = step_first_flg ? pconfig->arrays : pconfig->steps;
                o[0] = step_first_flg ? istep : iarray;
                o[1] = step_first_flg ? iarray : istep;
                init_write_buffer(wbuf, &my_rows, &my_cols, d, o);
#endif

                assert((fspace = H5Dget_space(dset)) >= 0);
                *create_time -= MPI_Wtime();
                create_selection(pconfig, fspace, my_proc_row, my_proc_col,
                                 istep, iarray);
                *create_time += MPI_Wtime();

                *write_time -= MPI_Wtime();

#if H5_VERSION_GE(1,13,0)
                if(es != NULL) {
                  assert(H5Dwrite_async(dset, H5T_NATIVE_DOUBLE, mspace, fspace, dxpl, wbuf, es->es_data) >= 0);
                }
                else
#endif
                  assert(H5Dwrite(dset, H5T_NATIVE_DOUBLE, mspace, fspace, dxpl, wbuf) >= 0);

                *write_time += MPI_Wtime();
                assert(H5Sclose(fspace) >= 0);
#if H5_VERSION_GE(1,13,0)
                if(es != NULL)
                  assert(H5Dclose_async(dset, es->es_meta_data) >= 0);
                else
#endif
                  assert(H5Dclose(dset) >= 0);
              }

            if (pconfig->async.enable == 1) {
              if (istep != pconfig->steps - 1) { // no sleep after the last ts
                if (rank == 0)
                  printf("Write Computing... \n");
                async_sleep(file, fapl, pconfig->async);
              }
              /* Even though we are writing the same data at each time step, normally we would need to 
               * fill the write buffer again before outputting the next time step. Here we
               * make sure write has completed before "filling" the write buffer again */
#if H5_VERSION_GE(1,13,0) 
              H5ESwait(es->es_data, H5ES_WAIT_FOREVER, &num_in_progress, &op_failed); 
#endif
            }

          }
      }
      break;
    default:
      break;
    }

  *create_time -= MPI_Wtime();
#if H5_VERSION_GE(1,13,0)
  if(es != NULL) {
    if (pconfig->async.enable == 1) {
#if H5_VERSION_GE(1,13,0)
      H5ESwait(es->es_meta_data, H5ES_WAIT_FOREVER, &num_in_progress, &op_failed);
#endif
      H5ESclose(es->es_meta_data);
      H5ESclose(es->es_data);
    }
    assert(H5Fclose_async(file, 0) >= 0);
    free(es);
  } else
#endif
    assert(H5Fclose(file) >= 0);

  *create_time += MPI_Wtime();
  assert(H5Sclose(mspace) >= 0);
  free(wbuf);
}
