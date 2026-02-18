#!/bin/bash

# Manage Samba Users (Add/Reset Password/Delete)
# Usage: 
#   ./manage_samba_user.sh add <username> <password>
#   ./manage_samba_user.sh password <username> <new_password>
#   ./manage_samba_user.sh delete <username>
#   ./manage_samba_user.sh list

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
COMPOSE_DIR="$SCRIPT_DIR/../docker"
COMPOSE_FILE="$COMPOSE_DIR/docker-compose.yml"

COMMAND="$1"
USERNAME="$2"
PASSWORD="$3"

if [ -z "$COMMAND" ]; then
    echo "Usage: $0 {add|password|delete|list} [username] [password]"
    exit 1
fi

case "$COMMAND" in
    add)
        if [ -z "$USERNAME" ] || [ -z "$PASSWORD" ]; then
            echo "Usage: $0 add <username> <password>"
            exit 1
        fi
        echo "Adding user '$USERNAME'..."
        docker compose -f "$COMPOSE_FILE" exec samba adduser -D -H -s /bin/false "$USERNAME" 2>/dev/null
        echo -e "$PASSWORD\n$PASSWORD" | docker compose -f "$COMPOSE_FILE" exec -T samba smbpasswd -a "$USERNAME"
        if [ $? -eq 0 ]; then echo "User '$USERNAME' added successfully."; else echo "Failed to add user."; fi
        ;;
    
    password)
        if [ -z "$USERNAME" ] || [ -z "$PASSWORD" ]; then
            echo "Usage: $0 password <username> <new_password>"
            exit 1
        fi
        echo "Updating password for '$USERNAME'..."
        echo -e "$PASSWORD\n$PASSWORD" | docker compose -f "$COMPOSE_FILE" exec -T samba smbpasswd "$USERNAME"
        if [ $? -eq 0 ]; then echo "Password updated successfully."; else echo "Failed to update password."; fi
        ;;
    
    delete)
        if [ -z "$USERNAME" ]; then
            echo "Usage: $0 delete <username>"
            exit 1
        fi
        echo "Deleting user '$USERNAME'..."
        docker compose -f "$COMPOSE_FILE" exec samba smbpasswd -x "$USERNAME"
        if [ $? -eq 0 ]; then echo "User '$USERNAME' deleted successfully."; else echo "Failed to delete user."; fi
        ;;

    list)
        echo "Samba Users:"
        docker compose -f "$COMPOSE_FILE" exec samba pdbedit -L -v | grep "Unix username"
        ;;
    
    *)
        echo "Unknown command: $COMMAND"
        echo "Usage: $0 {add|password|delete|list}"
        exit 1
        ;;
esac
