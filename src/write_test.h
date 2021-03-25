/* hdf5-iotest -- simple I/O performance tester for HDF5

   SPDX-License-Identifier: BSD-3-Clause

   Copyright (C) 2020, The HDF Group

   hdf5-iotest is released under the New BSD license (see COPYING).
   Go to the project home page for more info:

   https://github.com/HDFGroup/hdf5-iotest

*/

#ifndef WRITE_TEST_H
#define WRITE_TEST_H

#include "configuration.h"
#include "hdf5.h"

extern void write_test
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
 );

#endif
