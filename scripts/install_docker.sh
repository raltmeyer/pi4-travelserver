#!/bin/bash

# Ensure script is run as root
if [ "$EUID" -ne 0 ]; then
  echo "Please run as root"
  exit 1
fi

echo "Installing Docker..."
curl -fsSL https://get.docker.com -o get-docker.sh
sh get-docker.sh

echo "Adding user to docker group..."
# Assuming the default user is the one running sudo, or 'admin' / 'pi'
# We'll try to detect the user who invoked sudo
REAL_USER=${SUDO_USER:-$(whoami)}

usermod -aG docker "$REAL_USER"

echo "Docker installed."
echo "You must log out and log back in for group changes to take effect."
echo "Verifying installation..."
docker --version
docker compose version
