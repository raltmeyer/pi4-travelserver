#!/bin/bash

# Reset FileBrowser admin password
# Usage: ./reset_filebrowser_password.sh [new_password]

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
COMPOSE_DIR="$SCRIPT_DIR/../docker"

if [ -z "$1" ]; then
    echo "Usage: $0 <new_password>"
    exit 1
fi

NEW_PASS="$1"

echo "Stopping FileBrowser to release database lock..."
docker compose -f "$COMPOSE_DIR/docker-compose.yml" stop filebrowser

echo "Resetting 'admin' password..."
# Run a temporary container to update the DB securely
docker compose -f "$COMPOSE_DIR/docker-compose.yml" run --rm --entrypoint "filebrowser users update admin --password '$NEW_PASS'" filebrowser

if [ $? -eq 0 ]; then
    echo "Success! Password updated."
else
    echo "Error: Failed to update password."
fi

echo "Restarting FileBrowser..."
docker compose -f "$COMPOSE_DIR/docker-compose.yml" start filebrowser
