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

int check_options(configuration* pconfig, const char* section, const char* name, const char* value)
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
    strncpy(pconfig->scaling, value, 16);
  } else if (MATCH(section, "dataset-rank")) {
    pconfig->rank = (unsigned int) atoi(value);
  } else if (MATCH(section, "slowest-dimension")) {
    strncpy(pconfig->slowest_dimension, value, 16);
  } else if (MATCH(section, "alignment-increment")) {
    pconfig->alignment_increment = (hsize_t) atol(value);
  } else if (MATCH(section, "alignment-threshold")) {
    pconfig->alignment_threshold = (hsize_t) atol(value);
  } else if (MATCH(section, "layout")) {
    strncpy(pconfig->layout, value, 16);
  } else if (MATCH(section, "fill-values")) {
    strncpy(pconfig->fill_values, value, 8);
  } else if (MATCH(section, "mpi-io")) {
    strncpy(pconfig->mpi_io, value, 16);
  } else if (MATCH(section, "hdf5-file")) {
    strncpy(pconfig->hdf5_file, value, PATH_MAX);
  } else if (MATCH(section, "csv-file")) {
    strncpy(pconfig->csv_file, value, PATH_MAX);
  } else {
    return 0;  /* unknown name, error */
  }
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

int sanity_check(void* user)
{
  configuration* pconfig = (configuration*)user;

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
  assert(strncmp(pconfig->slowest_dimension, "step", 16) == 0 ||
         strncmp(pconfig->slowest_dimension, "array", 16) == 0);
  assert(strncmp(pconfig->layout, "contiguous", 16) == 0 ||
         strncmp(pconfig->layout, "chunked", 16) == 0);
  assert(strncmp(pconfig->fill_values, "true", 8) == 0 ||
         strncmp(pconfig->fill_values, "false", 8) == 0);
  assert(strncmp(pconfig->mpi_io, "independent", 16) == 0 ||
         strncmp(pconfig->mpi_io, "collective", 16) == 0);

  return 0;
}

/*
 *
 * Validate the configuration
 *
 */

int validate(void* user, const int size)
{
  configuration* pconfig = (configuration*)user;

  assert(pconfig->proc_rows*pconfig->proc_cols == (unsigned)size);

  if (strncmp(pconfig->scaling, "strong", 16) == 0) {
    assert(pconfig->rows%pconfig->proc_rows == 0);
    assert(pconfig->cols%pconfig->proc_cols == 0);
  }

  return 0;
}
