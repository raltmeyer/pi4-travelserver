#!/bin/bash

# Start the fileserver containers (FileBrowser + Samba)
# Containers must have been created first with create_fileserver.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
COMPOSE_DIR="$SCRIPT_DIR/../docker"

# Set defaults if not already exported
export MOUNT_POINT="${MOUNT_POINT:-/mnt/ssd}"
export TIMEZONE="${TIMEZONE:-America/Sao_Paulo}"

echo "Starting fileserver containers..."
docker compose -f "$COMPOSE_DIR/docker-compose.yml" start

echo ""
echo "=== Fileserver Status ==="
docker compose -f "$COMPOSE_DIR/docker-compose.yml" ps
echo ""
echo "FileBrowser: http://$(hostname -I | awk '{print $1}'):8080"
echo "Samba Share: \\\\$(hostname -I | awk '{print $1}')\\share"
