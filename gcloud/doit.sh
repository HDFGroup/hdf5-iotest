#!/bin/bash -x
#
# Build (parallel) HDF5
cd $HOME
git clone ssh://git@bitbucket.hdfgroup.org:7999/hdffv/hdf5.git
cd $HOME/hdf5
./autogen.sh
./configure --prefix=$HOME/.local --enable-build-mode=production \
  --enable-shared --enable-static --enable-optimization=high \
  --enable-parallel --disable-hl
make -j8
make install

# Build Darshan
cd $HOME
git clone https://xgitlab.cels.anl.gov/darshan/darshan.git
cd $HOME/darshan/darshan-runtime
./configure --prefix=$HOME/.local \
  --with-log-path=$HOME/darshan-logs --with-jobid-env=NONE \
  --enable-hdf5-mod=$HOME/.local
make -j8
make install
cd $HOME/darshan/darshan-util
./configure --prefix=$HOME/.local --enable-shared
make -j8
make install
cd $HOME/darshan/darshan-util/pydarshan
mkdir -p $HOME/.local/lib/python3.8/site-packages
python3 setup.py install --prefix=$HOME/.local
cd $HOME
mkdir -p $HOME/darshan-logs
$HOME/.local/bin/darshan-mk-log-dirs.pl

# Build Recorder 
cd $HOME
git clone https://github.com/uiuc-hpc/Recorder.git
cd $HOME/Recorder
./autogen.sh
./configure --prefix=$HOME/.local CFLAGS=-I$HOME/.local/include LDFLAGS=-L$HOME/.local/lib
make
make install

# Build the tester
cd $HOME/hdf5-iotest
./configure --prefix=$HOME/.local \
  --with-darshan=$HOME/.local \
  --with-gperf=/usr/lib/x86_64-linux-gnu \
  --with-hdf5=$HOME/.local
make
