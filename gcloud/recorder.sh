#!/bin/bash

prows=1
pcols=1

if [ "$1" != "" ]; then
  prows=$1
  if [ "$2" != "" ]; then
    pcols=$2
  fi
fi

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
        p=$((prows*pcols))
	mkdir -p $count
	cd $count
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
        LD_PRELOAD=$HOME/.local/lib/librecorder.so \
	mpiexec -n $p ../hdf5_iotest $file > $out
	python3 $HOME/.local/bin/reporter/reporter.py ./recorder-logs
	cp recorder-report.html ../recorder-report-$count.html
	cd ..
        rm -rf $count
        ((++count))
      done
    done
  done
done
