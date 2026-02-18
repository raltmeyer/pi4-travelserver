

echo "Updating system..."
sudo apt update && sudo apt full-upgrade -y

echo "Installing iPhone USB tethering support..."
sudo apt install -y usbmuxd libimobiledevice-1.0-6 libimobiledevice-utils ipheth-utils

echo "Installing exFAT utilities..."
sudo apt-get install -y exfat-fuse exfatprogs

echo "Installing raspap..."
#curl -sL https://install.raspap.com | bash

