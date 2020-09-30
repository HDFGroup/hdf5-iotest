#!/bin/bash -x
#
# Create a GCloud instance with two local SSDs
gcloud compute instances create gcloud-instance \
  --machine-type n2-standard-8 \
  --local-ssd interface=nvme \
  --local-ssd interface=nvme \
  --local-ssd interface=nvme \
  --local-ssd interface=nvme \
  --image-project ubuntu-os-cloud \
  --image-family ubuntu-minimal-2004-lts \
  --preemptible --address 34.121.21.163 \
  --metadata-from-file startup-script=gcloud-startup-script.sh
