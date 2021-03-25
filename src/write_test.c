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

void write_test
(
 configuration* pconfig,
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
  d[2] = strong_scaling_flg ? config.rows : config.rows * config.proc_rows;
  d[3] = strong_scaling_flg ? config.cols : config.cols * config.proc_cols;

  o[2] = strong_scaling_flg ? rank * my_rows : my_proc_row * config.rows;
  o[3] = strong_scaling_flg ? rank * my_cols : my_proc_col * config.cols;

  printf("\nWARNING: Data verification enabled. Timings will be distorted!!!\n");
#else
  for (i = 0; i < (size_t)my_rows*my_cols; ++i)
    wbuf[i] = (double) (my_proc_row + my_proc_col);
#endif

  *create_time -= MPI_Wtime();
  assert((file = H5Fcreate(pconfig->hdf5_file, H5F_ACC_TRUNC, fcpl, fapl))
         >= 0);
  *create_time += MPI_Wtime();

  switch (pconfig->rank)
    {
    case 4:
      {
        /* a single 4D array */
        *create_time -= MPI_Wtime();
        assert((dset = create_dataset(pconfig, file, "dataset", lcpl, dapl))
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
                create_selection(pconfig, fspace, my_proc_row, my_proc_col,
                                 istep, iarray);

                *write_time -= MPI_Wtime();
                assert(H5Dwrite(dset, H5T_NATIVE_DOUBLE, mspace, fspace,
                                dxpl, wbuf) >= 0);
                *write_time += MPI_Wtime();
                assert(H5Sclose(fspace) >= 0);
              }
          }

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
                assert((dset = create_dataset(pconfig, file, path, lcpl, dapl))
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
                    create_selection(pconfig, fspace, my_proc_row,
                                     my_proc_col, istep, iarray);

                    *write_time -= MPI_Wtime();
                    assert(H5Dwrite(dset, H5T_NATIVE_DOUBLE, mspace, fspace,
                                    dxpl, wbuf) >= 0);
                    *write_time += MPI_Wtime();
                    assert(H5Sclose(fspace) >= 0);
                  }

                assert(H5Dclose(dset) >= 0);
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
                                                    lcpl, dapl)) >= 0);
                    *create_time += MPI_Wtime();

#ifdef VERIFY_DATA
                    d[0] = pconfig->arrays; d[1] = pconfig->steps;
                    o[0] = iarray; o[1] = istep;
                    init_write_buffer(wbuf, &my_rows, &my_cols, d, o);
#endif
                    assert((fspace = H5Dget_space(dset)) >= 0);
                    create_selection(pconfig, fspace, my_proc_row,
                                     my_proc_col, istep, iarray);

                    *write_time -= MPI_Wtime();
                    assert(H5Dwrite(dset, H5T_NATIVE_DOUBLE, mspace, fspace,
                                    dxpl, wbuf) >= 0);
                    *write_time += MPI_Wtime();
                    assert(H5Sclose(fspace) >= 0);
                    assert(H5Dclose(dset) >= 0);
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
                                              lcpl, dapl)) >= 0);
                *create_time += MPI_Wtime();

#ifdef VERIFY_DATA
                d[0] = step_first_flg ? pconfig->steps : pconfig->arrays;
                d[1] = step_first_flg ? pconfig->arrays : pconfig->steps;
                o[0] = step_first_flg ? istep : iarray;
                o[1] = step_first_flg ? iarray : istep;
                init_write_buffer(wbuf, &my_rows, &my_cols, d, o);
#endif

                assert((fspace = H5Dget_space(dset)) >= 0);
                create_selection(pconfig, fspace, my_proc_row, my_proc_col,
                                 istep, iarray);

                *write_time -= MPI_Wtime();
                assert(H5Dwrite(dset, H5T_NATIVE_DOUBLE, mspace, fspace,
                                dxpl, wbuf) >= 0);
                *write_time += MPI_Wtime();
                assert(H5Sclose(fspace) >= 0);
                H5Dclose(dset);
              }
          }
      }
      break;
    default:
      break;
    }
  *create_time -= MPI_Wtime();
  assert(H5Fclose(file) >= 0);
  *create_time += MPI_Wtime();
  assert(H5Sclose(mspace) >= 0);
  free(wbuf);
}
