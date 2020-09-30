#!/bin/bash -x
#
# Create a RAID-0 volume
sudo mdadm --create /dev/md0 --level=0 --raid-devices=2 \
  /dev/nvme0n1 /dev/nvme0n2

sudo mkfs.ext4 -F /dev/md0

sudo mkdir -p /mnt/scratch

sudo mount -o discard,defaults,nobarrier /dev/md0 /mnt/scratch

sudo sudo chmod a+w /mnt/scratch

# full write pass - measures write bandwidth with 1M blocksize
fio --name=writefile --size=100G --filesize=100G \
  --filename=/mnt/scratch/google-local-nvme-ssd-r0 --bs=1M --nrfiles=1 \
  --direct=1 --sync=0 --randrepeat=0 --rw=write --refill_buffers \
  --end_fsync=1 --iodepth=200 --ioengine=libaio

# rand read - measures max read IOPS with 4k blocks
fio --time_based --name=benchmark --size=100G --runtime=30 \
  --filename=/mnt/scratch/google-local-nvme-ssd-r0 --ioengine=libaio \
  --randrepeat=0 --iodepth=128 --direct=1 --invalidate=1 --verify=0 \
  --verify_fatal=0 --numjobs=4 --rw=randread --blocksize=4k --group_reporting

# rand write - measures max write IOPS with 4k blocks
fio --time_based --name=benchmark --size=100G --runtime=30 \
  --filename=/mnt/scratch/google-local-nvme-ssd-r0 --ioengine=libaio \
  --randrepeat=0 --iodepth=128 --direct=1 --invalidate=1 --verify=0 \
  --verify_fatal=0 --numjobs=4 --rw=randwrite --blocksize=4k --group_reporting
