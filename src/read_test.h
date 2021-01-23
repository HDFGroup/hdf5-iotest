#ifndef READ_TEST_H
#define READ_TEST_H

#include "configuration.h"
#include "hdf5.h"

extern void read_test
(
 configuration* pconfig,
 int size,
 int rank,
 int my_proc_row,
 int my_proc_col,
 unsigned long my_rows,
 unsigned long my_cols,
 hid_t fapl,
 hid_t mspace,
 hid_t dxpl,
 double* read_time
 );

#endif
