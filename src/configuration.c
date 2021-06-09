/* hdf5-iotest -- simple I/O performance tester for HDF5

   SPDX-License-Identifier: BSD-3-Clause

   Copyright (C) 2020, The HDF Group

   hdf5-iotest is released under the New BSD license (see COPYING).
   Go to the project home page for more info:

   https://github.com/HDFGroup/hdf5-iotest

*/

#include "configuration.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*
 *
 * Handle parameter conversion
 *
 */

int check_options
(
 configuration* pconfig,
 const char* section,
 const char* name,
 const char* value
 )
{
#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

  if (MATCH(section, "steps")) {
    pconfig->steps = (unsigned int) atoi(value);
  } else if (MATCH(section, "arrays")) {
    pconfig->arrays = (unsigned int) atoi(value);
  } else if (MATCH(section, "rows")) {
    pconfig->rows = (unsigned long) atol(value);
  } else if (MATCH(section, "columns")) {
    pconfig->cols = (unsigned long) atol(value);
  } else if (MATCH(section, "process-rows")) {
    pconfig->proc_rows = (unsigned int) atoi(value);
  } else if (MATCH(section, "process-columns")) {
    pconfig->proc_cols = (unsigned int) atoi(value);
  } else if (MATCH(section, "scaling")) {
    strncpy(pconfig->scaling, value, 15);
  } else if (MATCH(section, "dataset-rank")) {
    pconfig->rank = (unsigned int) atoi(value);
  } else if (MATCH(section, "slowest-dimension")) {
    strncpy(pconfig->slowest_dimension, value, 15);
  } else if (MATCH(section, "libver-bound-low")) {
    strncpy(pconfig->libver_bound_low, value, 15);
  } else if (MATCH(section, "libver-bound-high")) {
    strncpy(pconfig->libver_bound_high, value, 15);
  } else if (MATCH(section, "alignment-increment")) {
    pconfig->alignment_increment = (hsize_t) atol(value);
  } else if (MATCH(section, "alignment-threshold")) {
    pconfig->alignment_threshold = (hsize_t) atol(value);
  } else if (MATCH(section, "meta-block-size")) {
    pconfig->meta_block_size = (hsize_t) atol(value);
  } else if (MATCH(section, "layout")) {
    strncpy(pconfig->layout, value, 15);
  } else if (MATCH(section, "fill-values")) {
    strncpy(pconfig->fill_values, value, 7);
  } else if (MATCH(section, "single-process")) {
    strncpy(pconfig->single_process, value, 15);
  } else if (MATCH(section, "mpi-io")) {
    strncpy(pconfig->mpi_io, value, 15);
  } else if (MATCH(section, "split")) {
    pconfig->split = (unsigned int) atol(value);
  } else if (MATCH(section, "hdf5-file")) {
    strncpy(pconfig->hdf5_file, value, PATH_MAX-1);
    /* Enable individual output HDF5 files per case, denoted by a "#" in the filename */
    if(strstr(pconfig->hdf5_file, "#") != NULL)
      {
        pconfig->HDF5perCase = 1;
      }
  } else if (MATCH(section, "csv-file")) {
    strncpy(pconfig->csv_file, value, PATH_MAX-1);
  } else if (MATCH(section, "restart")) {
    pconfig->restart = (unsigned int) atol(value);
  } else if (MATCH(section, "one-case")) {
    pconfig->one_case = (unsigned int) atol(value);
  } else if (MATCH(section, "gzip")) {
    strncpy(pconfig->compress_type, "gzip", 15);
    pconfig->compress_par[0] = (unsigned int) atol(value);
  } else if (MATCH(section, "szip")) {
    strncpy(pconfig->compress_type, "szip", 15);
    char tmp[32];
    strncpy(tmp, value, 31);
    char * pch;
    pch = strtok (tmp, " ,");
    int icnt = 0;
    while (pch != NULL) {
      if(icnt == 0) {
        if ( strcmp(pch,"H5_SZIP_EC_OPTION_MASK") ) {
          pconfig->compress_par[icnt] = H5_SZIP_EC_OPTION_MASK;
        } else if ( strcmp(pch,"H5_SZIP_NN_OPTION_MASK") ) {
          pconfig->compress_par[icnt] = H5_SZIP_NN_OPTION_MASK;
        } else {
          return 0;  /* invalid parameter, error */
        }
      } else if(icnt == 1) {
        pconfig->compress_par[icnt] = (unsigned int) atol(pch);
      }
      pch = strtok (NULL, " ,");
      icnt++;
    }
  } else {
    return 0;  /* unknown name, error */
  }

  return 1;
}

int handler(void* user,
            const char* section,
            const char* name,
            const char* value)
{
  configuration* pconfig = (configuration*)user;

  if (strncmp(section, "DEFAULT", 7) == 0)
    {
      if (strcmp(section, "DEFAULT") == 0 && strcmp(name, "version") == 0)
        pconfig->version = atoi(value);
      else
        check_options(pconfig, "DEFAULT", name, value);
    }
  else
    {
      check_options(pconfig, section, name, value);
    }

  return 1;
}

/*
 *
 * Check if the parameters have sensible values
 *
 */

int validate(configuration* pconfig, const int size)
{
  htri_t avail;
  herr_t status;
  unsigned int filter_info;

  assert(pconfig->version == 0);
  assert(pconfig->steps > 0);
  assert(pconfig->arrays > 0);
  assert(pconfig->rows > 1);
  assert(pconfig->proc_rows >= 1);
  assert(pconfig->cols > 1);
  assert(pconfig->proc_cols >= 1);
  assert(strncmp(pconfig->scaling, "weak", 16) == 0 ||
         strncmp(pconfig->scaling, "strong", 16) == 0);
  assert(pconfig->rank > 1 && pconfig->rank < 5);

  assert(pconfig->proc_rows*pconfig->proc_cols == (unsigned)size);

  if (strncmp(pconfig->scaling, "strong", 16) == 0) {
    assert(pconfig->rows%pconfig->proc_rows == 0);
    assert(pconfig->cols%pconfig->proc_cols == 0);
  }

  assert(pconfig->alignment_increment >= 1);

  assert(strncmp(pconfig->single_process, "posix", 16) == 0 ||
         strncmp(pconfig->single_process, "core", 16) == 0  ||
         strncmp(pconfig->single_process, "mpi-io-uni", 16) == 0);

  assert(pconfig->restart == 0 || pconfig->restart == 1);
  assert(pconfig->split == 0 || pconfig->split == 1);
  assert(pconfig->one_case >= 0);

  if (strncmp(pconfig->compress_type, "gzip", 16) == 0) {
    /* check if gzip compression is available */
    avail = H5Zfilter_avail(H5Z_FILTER_DEFLATE);
    assert(avail > 0 );

    status = H5Zget_filter_info (H5Z_FILTER_DEFLATE, &filter_info);
    assert(status >= 0);
    assert( (filter_info & H5Z_FILTER_CONFIG_ENCODE_ENABLED) ||
            (filter_info & H5Z_FILTER_CONFIG_DECODE_ENABLED) );
  } else if (strncmp(pconfig->compress_type, "szip", 16) == 0) {
    /* check if szip compression is available */
    avail = H5Zfilter_avail(H5Z_FILTER_SZIP);
    assert(avail > 0 );

    status = H5Zget_filter_info (H5Z_FILTER_SZIP, &filter_info);
    assert(status >= 0);

    assert( (filter_info & H5Z_FILTER_CONFIG_ENCODE_ENABLED) ||
            (filter_info & H5Z_FILTER_CONFIG_DECODE_ENABLED) );
  }

  return 0;
}
