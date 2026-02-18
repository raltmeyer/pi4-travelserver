#!/bin/bash

# Stop the fileserver containers (FileBrowser + Samba)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
COMPOSE_DIR="$SCRIPT_DIR/../docker"

echo "Stopping fileserver containers..."
docker compose -f "$COMPOSE_DIR/docker-compose.yml" stop

echo "Containers stopped."
docker compose -f "$COMPOSE_DIR/docker-compose.yml" ps
