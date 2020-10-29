/* hdf5-iotest -- simple I/O performance tester for HDF5

   SPDX-License-Identifier: BSD-3-Clause

   Copyright (C) 2020, The HDF Group

   hdf5-iotest is released under the New BSD license (see COPYING).
   Go to the project home page for more info:

   https://github.com/HDFGroup/hdf5-iotest

*/

#ifndef DATASET_H
#define DATASET_H

#include "configuration.h"

#include "hdf5.h"

extern hid_t create_dataset(const configuration* config,
                            hid_t file,
                            const char* name);

extern int create_selection(const configuration* config,
                            hid_t fspace,
                            const int proc_row,
                            const int proc_col,
                            const unsigned int step,
                            const unsigned int array);

#endif
