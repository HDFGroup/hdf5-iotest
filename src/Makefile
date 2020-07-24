
CC = mpicc
HDF5_DIR ?= /home/gerdheber/.local
DARSHAN_DIR ?= /home/gerdheber/.local
PROFILER_LIB_DIR ?= /usr/lib/x86_64-linux-gnu

CFLAGS = -Wextra -O2 -I$(HDF5_DIR)/include

LDFLAGS := ${LDFLAGS}
LDFLAGS += -L${HDF5_DIR}/lib -lhdf5 \
	   -Wl,-no-as-needed -L$(PROFILER_LIB_DIR) -lprofiler

OBJECTS := hdf5_iotest.o ini.o dataset.o configuration.o

OUTPUT_PATH := pprof

all: hdf5_iotest

hdf5_iotest: $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) -lhdf5 -lmpi

%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f *.o *.h5

veryclean: clean
	rm -f hdf5_iotest baseline.ini hdf5_iotest.csv random-write.0.0
	rm -rf pprof

whats_mounted:
	mount | grep /dev

block_devs:
	lsblk

os_info:
	uname -a

cc_info:
	$(CC) --version

mpi_info:
	mpichversion

hdf5_version:
	$(HDF5_DIR)/bin/h5ls --version | cut --d ' ' -f 3

run_fio:
	fio --name=random-write --ioengine=posixaio \
		--rw=randwrite --bs=1m --size=4g --numjobs=1 --iodepth=1 \
		--runtime=60 --time_based --end_fsync=1

baseline: hdf5_iotest
	LD_LIBRARY_PATH=$(HDF5_DIR)/lib ./hdf5_iotest baseline.ini
	cat hdf5_iotest.csv

h5dump:
	$(HDF5_DIR)/bin/h5dump -pBH hdf5_iotest.h5

h5stat:
	$(HDF5_DIR)/bin/h5stat hdf5_iotest.h5

gprofile: hdf5_iotest
	mkdir -p ${OUTPUT_PATH}
	rm -f ${OUTPUT_PATH}/*.out
	CPUPROFILE_FREQUENCY=10000 \
	CPUPROFILE_REALTIME=1 \
	CPUPROFILE=/tmp/pprof.out \
	LD_LIBRARY_PATH=$(HDF5_DIR)/lib ./hdf5_iotest baseline.ini ; \
	google-pprof --callgrind ./hdf5_iotest /tmp/pprof.out > \
		${OUTPUT_PATH}/callgrind.out
	callgrind_annotate pprof/callgrind.out | head -n 50 | tail -n 38

darshan: hdf5_iotest
	rm -f /tmp/*.darshan
	LD_PRELOAD=$(DARSHAN_DIR)/lib/libdarshan.so:$(HDF5_DIR)/lib/libhdf5.so \
		DARSHAN_LOG_DIR=/tmp ./hdf5_iotest baseline.ini
	$(DARSHAN_DIR)/bin/darshan-parser --all /tmp/*.darshan
