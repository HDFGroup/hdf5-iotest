bin_PROGRAMS = hdf5_iotest

dist_pkgdata_DATA = hdf5_iotest.ini combinator.sh

hdf5_iotest_SOURCES = configuration.c dataset.c hdf5_iotest.c ini.c read_test.c \
	utils.c write_test.c

hdf5_iotest_LDADD = -lhdf5 -luuid
