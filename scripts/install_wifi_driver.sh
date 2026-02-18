#!/bin/bash

# Ensure script is run as root
if [ "$EUID" -ne 0 ]; then
  echo "Please run as root"
  exit 1
fi

echo "Updating package list and upgrading system..."
apt update && apt upgrade -y

echo "Installing kernel headers and build tools..."
# Try specific RPi headers first, then fall back to generic kernel headers matching running kernel
if apt-cache search raspberrypi-kernel-headers | grep -q raspberrypi-kernel-headers; then
    HEADERS_PKG="raspberrypi-kernel-headers"
else
    HEADERS_PKG="linux-headers-$(uname -r)"
fi

echo "Detected Kernel Headers Package: $HEADERS_PKG"
apt install -y "$HEADERS_PKG" build-essential bc dkms git usb-modeswitch

echo "Cloning RTL8821AU driver repository..."
# Using morrownr's repo as it is well-maintained for Linux 5.x/6.x kernels
mkdir -p /usr/src
cd /usr/src
if [ -d "8821au-20210708" ]; then
    echo "Directory already exists, removing old version..."
    rm -rf 8821au-20210708
fi

git clone https://github.com/morrownr/8821au-20210708.git
cd 8821au-20210708

echo "Running installation script..."
# The authorized installation script from the repo handles DKMS dkms.conf generation
./install-driver.sh

echo "Installation complete."
echo "You MUST reboot for the changes to take effect."
