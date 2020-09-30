#!/bin/bash -x
#
# Log file on AWS EC2:
#   tail -f /var/log/cloud-init-output.log
# On Google Cloud:
#   journalctl -o cat -f _SYSTEMD_UNIT=google-startup-scripts.service
#
# Configure the following environmental variables as required:
INSTALL_CHROME=no
FORMAT_SCRATCH=no
# EC2
SCRATCH_DEVICE=
# GC
# SCRATCH_DEVICE=sdb
SCRATCH_MOUNT=/mnt/scratch

# Any additional packages that should be installed on startup can be added here
EXTRA_PACKAGES="less bzip2 zip unzip wget"
EXTRA_PACKAGES="$EXTRA_PACKAGES vim ctags emacs"
EXTRA_PACKAGES="$EXTRA_PACKAGES autoconf automake libtool-bin"
EXTRA_PACKAGES="$EXTRA_PACKAGES gcc g++ llvm clang make cmake"
EXTRA_PACKAGES="$EXTRA_PACKAGES libz-dev libssl-dev libcurl4-openssl-dev"
EXTRA_PACKAGES="$EXTRA_PACKAGES google-perftools libgoogle-perftools-dev"
EXTRA_PACKAGES="$EXTRA_PACKAGES gdb valgrind kcachegrind"
EXTRA_PACKAGES="$EXTRA_PACKAGES gnuplot fio mpich"
EXTRA_PACKAGES="$EXTRA_PACKAGES texlive texlive-font-utils texlive-latex-extra"
EXTRA_PACKAGES="$EXTRA_PACKAGES python3-pip python3-tk python3-venv"

function install_default_env {
  PACKAGES="tmux xauth x11-apps default-jre git"

  DEBIAN_FRONTEND=noninteractive \
    apt-get install --assume-yes $PACKAGES $EXTRA_PACKAGES
}

function mount_scratch { # args DEVICE MOUNT_POINT
  declare device="$1" mount_point="$2"
  [[ "$FORMAT_SCRATCH" = "yes" ]] && \
    mkfs -t ext4 /dev/$device
  mkdir $mount_point
  mount /dev/$device $mount_point
}

function download_and_install { # args URL FILENAME
  curl -L -o "$2" "$1"
  dpkg --install "$2"
  apt-get install --assume-yes --fix-broken
}

function is_installed {  # args PACKAGE_NAME
  dpkg-query --list "$1" | grep -q "^ii" 2>/dev/null
  return $?
}

# Check if we have a scratch drive & mount
SCRATCH=$(lsblk | grep $SCRATCH_DEVICE | awk '{print $1}')

[[ "$SCRATCH" = "$SCRATCH_DEVICE" ]] && \
  mount_scratch $SCRATCH_DEVICE $SCRATCH_MOUNT

apt-get update

install_default_env

popular_python

[[ "$INSTALL_CHROME" = "yes" ]] && \
  ! is_installed google-chrome-stable && \
  download_and_install \
    https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb \
    /tmp/google-chrome-stable_current_amd64.deb

echo "gcloud-startup-script.sh completed"
