#!/bin/bash

# Configuration
MOUNT_POINT="/mnt/ssd"

# Ensure script is run as root
if [ "$EUID" -ne 0 ]; then
  echo "Please run as root"
  exit 1
fi

echo "Available Block Devices:"
lsblk
echo "---------------------------------------------------"

echo "This script will mount an exFAT device and add it to fstab."
echo "Enter the device name (e.g., sda, sdb, sda1):"
read -p "/dev/" DRIVE_NAME

DEVICE="/dev/$DRIVE_NAME"

# Prevent mounting the SD card
if [[ "$DRIVE_NAME" == "mmcblk0" || "$DRIVE_NAME" == mmcblk0* ]]; then
    echo "ERROR: You selected the SD card! Aborting."
    exit 1
fi

if [ ! -b "$DEVICE" ]; then
    echo "Error: Device $DEVICE not found."
    exit 1
fi

# Verify it's exFAT
FSTYPE=$(blkid -s TYPE -o value "$DEVICE")
if [ "$FSTYPE" != "exfat" ]; then
    echo "Error: $DEVICE is not exFAT (detected: ${FSTYPE:-unknown})."
    echo "This script only supports exFAT devices."
    exit 1
fi

# Get UUID
UUID=$(blkid -s UUID -o value "$DEVICE")
if [ -z "$UUID" ]; then
    echo "Error: Could not get UUID for $DEVICE."
    exit 1
fi

echo "Device:     $DEVICE"
echo "Filesystem: $FSTYPE"
echo "UUID:       $UUID"
echo "Mount:      $MOUNT_POINT"

# Create mount point
mkdir -p "$MOUNT_POINT"

# Check if already in fstab
if grep -q "$UUID" /etc/fstab; then
    echo "UUID $UUID is already in /etc/fstab. Skipping fstab update."
else
    echo "Adding to /etc/fstab..."
    # Remove any old entry for the same mount point to prevent conflicts
    sed -i "\|\s$MOUNT_POINT\s|d" /etc/fstab
    # exFAT mount options:
    # uid=1000,gid=1000 - default user ownership
    # umask=000 - rwx for everyone (for file sharing)
    # nofail - don't block boot if drive is missing
    echo "UUID=$UUID $MOUNT_POINT exfat defaults,auto,users,rw,nofail,uid=1000,gid=1000,umask=000 0 0" >> /etc/fstab
    echo "Entry added to /etc/fstab."
fi

# Mount
systemctl daemon-reload
mount -a

if mountpoint -q "$MOUNT_POINT"; then
    echo "Successfully mounted $DEVICE at $MOUNT_POINT."
else
    echo "Error: Failed to mount. Check dmesg for details."
    exit 1
fi

echo "Setup complete!"
