#!/bin/bash

# Create and start the fileserver containers (FileBrowser + Samba)
# Uses docker-compose.yml in the docker/ directory

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
COMPOSE_DIR="$SCRIPT_DIR/../docker"

# Ensure run as root
if [ "$EUID" -ne 0 ]; then
  echo "Please run as root"
  exit 1
fi

# Set defaults if not already exported
export MOUNT_POINT="${MOUNT_POINT:-/mnt/ssd}"
export TIMEZONE="${TIMEZONE:-America/Sao_Paulo}"

# Verify mount point exists and is mounted
if ! mountpoint -q "$MOUNT_POINT"; then
    echo "Warning: $MOUNT_POINT is not mounted."
    echo "Run setup_fstab.sh first to mount your storage."
    exit 1
fi

# Create files directory if it doesn't exist
mkdir -p "$MOUNT_POINT/files"
chown 1000:1000 "$MOUNT_POINT/files"

# --- PERMANENT FIX: Pre-create files to avoid Docker creating directories ---
DB_FILE="$COMPOSE_DIR/filebrowser.db"
JSON_FILE="$COMPOSE_DIR/.filebrowser.json"

if [ ! -f "$DB_FILE" ]; then
    echo "Initializing empty database file..."
    touch "$DB_FILE"
    chmod 666 "$DB_FILE"
fi

if [ ! -f "$JSON_FILE" ]; then
    echo "Initializing default config file..."
    echo '{}' > "$JSON_FILE"
    chmod 666 "$JSON_FILE"
fi
# ---------------------------------------------------------------------------

echo "Creating and starting fileserver containers..."
docker compose -f "$COMPOSE_DIR/docker-compose.yml" up -d

echo ""
echo "=== Fileserver Status ==="
docker compose -f "$COMPOSE_DIR/docker-compose.yml" ps
echo ""
echo "FileBrowser: http://$(hostname -I | awk '{print $1}'):8080"
echo "Samba Share: \\\\$(hostname -I | awk '{print $1}')\\share"
