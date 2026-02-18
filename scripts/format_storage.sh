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

echo "WARNING: This script will FORMAT the selected drive to exFAT."
echo "ALL DATA ON THE DRIVE WILL BE LOST."
echo "Enter the device name (e.g., sda, sdb) NOT partition (sda1):"
read -p "/dev/" DRIVE_NAME

DEVICE="/dev/$DRIVE_NAME"

# specific check to avoid formatting the SD card (mmcblk0) or system drive if identifiable
if [[ "$DRIVE_NAME" == "mmcblk0" ]]; then
    echo "ERROR: You selected the SD card! Aborting to prevent system destruction."
    exit 1
fi

if [ ! -b "$DEVICE" ]; then
    echo "Error: Device $DEVICE not found."
    exit 1
fi

echo "You selected $DEVICE."
read -p "Are you ABSOLUTELY SURE you want to format $DEVICE? (yes/NO): " CONFIRM

if [ "$CONFIRM" != "yes" ]; then
    echo "Aborted."
    exit 0
fi

# 1. Formatting (Superfloppy - No Partition Table)
# This avoids MBR/GPT issues on macOS for removable drives
echo "Formatting ENTIRE device $DEVICE to exFAT (Superfloppy)..."
mkfs.exfat -n "TravelDrive" "$DEVICE"

PARTITION="$DEVICE"
sleep 2

# 3. Mounting
echo "Creating mount point at $MOUNT_POINT..."
mkdir -p "$MOUNT_POINT"

# Get UUID
UUID=$(blkid -s UUID -o value "$PARTITION")
if [ -z "$UUID" ]; then
    echo "Error: Could not get UUID."
    exit 1
fi

echo "Detected UUID: $UUID"

# 4. Configure fstab
# Remove old entry if exists to prevent duplicates/conflicts
sed -i "\|\s$MOUNT_POINT\s|d" /etc/fstab

echo "Adding to /etc/fstab..."
# exFAT needs specific options for permissions since it doesn't support them natively
# uid=1000,gid=1000 makes the default user the owner
# umask=000 gives rwx to everyone (simplest for a travel server file share)
echo "UUID=$UUID $MOUNT_POINT exfat defaults,auto,users,rw,nofail,uid=1000,gid=1000,umask=000 0 0" >> /etc/fstab

# 5. Mount and Set Permissions
systemctl daemon-reload
mount -a

if mountpoint -q "$MOUNT_POINT"; then
    echo "Successfully mounted."
    echo "Permissions set via mount options (uid=1000, umask=000)."
else
    echo "Error: Failed to mount."
    exit 1
fi

echo "Storage setup complete!"
