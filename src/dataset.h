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

typedef enum ts_status { TS_INIT, TS_DELAY, TS_READY, TS_DONE } ts_status;

typedef struct time_step time_step;

struct time_step {
    hid_t              es_meta_create;
    hid_t              es_meta_close;
    hid_t              es_data;
    hid_t              es_meta_data;
    hid_t              dset_ids[8];
    ts_status          status;
};

extern hid_t create_dcpl(const configuration* config, unsigned int coll_mpi_io_flg);

extern hid_t create_dataset(const configuration* config,
                            hid_t file,
                            const char* name,
                            hid_t lcpl,
                            hid_t dapl,
                            unsigned int coll_mpi_io_flg,
                            time_step *ts);

extern int create_selection(const configuration* config,
                            hid_t fspace,
                            const int proc_row,
                            const int proc_col,
                            const unsigned int step,
                            const unsigned int array);

extern void init_write_buffer(double wbuf[],
                              const size_t* my_rows,
                              const size_t* my_cols,
                              size_t d[],
                              size_t o[]);

extern void verify_read_buffer(double* rbuf,
                               const size_t* my_rows,
                               const size_t* my_cols,
                               size_t d[],
                               size_t o[]);

extern void async_sleep(hid_t file_id, 
                        hid_t fapl, 
                        duration sleep_time);

#endif
