#!/bin/bash

prows=1
pcols=1

ini="FIXME!"

if [ -f hdf5_iotest.ini ]
then
  ini=$(<hdf5_iotest.ini)
fi

count=1

for rk in 2 3 4
do
  for sd in step array
  do
    for lo in contiguous chunked
    do
      for io in independent collective
      do
        file="run-$count.ini"
        {
          echo "$ini"
          echo -ne "\n[combinator]\n"
          echo "process-rows = $prows"
          echo "process-columns = $pcols"
          echo "dataset-rank = $rk"
          echo "slowest-dimension = $sd"
          echo "layout = $lo"
          echo "mpi-io = $io"
          echo "hdf5-file = out-$count.h5"
          echo "csv-file = out-$count.csv"
        } >> $file 
        out="out-$count.txt"  
        hdf="out-$count.h5"  
        p=$((prows*pcols))
        CPUPROFILE_FREQUENCY=10000 \
        CPUPROFILE_REALTIME=1 \
	CPUPROFILE=/tmp/pprof.out \
	./hdf5_iotest $file > $out
	google-pprof --callgrind ./hdf5_iotest /tmp/pprof.out > \
  	callgrind.$count.out
        rm $hdf $file
        ((++count))
      done
    done
  done
done
