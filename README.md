# Pi4 Travel Server

A complete solution for a portable Raspberry Pi 4 server featuring:
- **RaspAP**: Wi-Fi hotspot and network management.
- **FileBrowser**: Web-based file manager (`http://10.3.141.1:8080`).
- **Samba**: Network file share (`\\10.3.141.1\share`).
- **Docker**: Containerized services for easy management.

---

## 🛠 Installation Guide (From Scratch)

Follow these steps to set up your server starting with a fresh Raspberry Pi OS Lite image.

### 1. Prepare the SD Card
1.  Download **Raspberry Pi OS Lite (64-bit)**.
2.  Flash it to your SD card using **Raspberry Pi Imager**.
3.  **Before ejecting**, configure settings in Imager (Gear icon):
    -   Host: `travelpi`
    -   User: `user`
    -   Pass: `UserPass` (or your choice)
    -   Enable SSH.
    -   Configure Wi-Fi (optional, for initial internet access).

### 2. Initial Setup
Insert SD card, boot the Pi, and SSH into it:
```bash
ssh user@travelpi.local
# or
ssh user@<ip-address>
```

Update system and install git:
```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y git
```

### 3. Clone this Repository
```bash
cd ~
git clone https://github.com/user/pi4-travelserver.git
cd pi4-travelserver
chmod +x scripts/*.sh
```

### 4. Run Installation Scripts
Execute these scripts in order:

**A. Install Base Dependencies**
Installs core tools (curl, wget, vim, etc.).
```bash
sudo ./scripts/install_base.sh
```

**B. Install Docker**
Installs Docker Engine and Docker Compose.
```bash
sudo ./scripts/install_docker.sh
# You may need to logout and login again for group changes to take effect
newgrp docker 
```

**C. Install Wi-Fi Drivers (Optional)**
Only needed if you use a Realtek USB Wi-Fi adapter (e.g., RTL8812AU). Detailed prompts included.
```bash
sudo ./scripts/install_wifi_driver.sh
```

**D. Configure Storage**
Formats your external USB drive to exFAT (if needed) and sets up auto-mounting.
*Warning: Formatting wipes all data! Only run format if the drive is new.*
```bash
# To just mount an existing drive:
sudo ./scripts/install_mnt_fstab.sh
```

### 5. deploy Services
Starts FileBrowser and Samba containers.
```bash
sudo ./scripts/create_fileserver.sh
```

### 6. Configure Access
**Important:** Samba guest access is disabled for security. You must create a user.

```bash
# Create Samba user (Access to \\10.3.141.1\share)
sudo ./scripts/manage_samba_user.sh add <username> <password>

# Reset FileBrowser admin password (Access to http://10.3.141.1:8080)
sudo ./scripts/reset_filebrowser_password.sh <new_password>
```

### 7. Setup the two wifi networks

```
sudo ./scripts/full_network_reset.sh
```

### 8. Firewall configuration

```
sudo ./scripts/firewall_strict.sh
```
or
```
sudo ./scripts/firewall_maintenance.sh
```

---

## 📂 Usage

| Service | Address | Default Credentials |
|---|---|---|
| **Web Dashboard** (RaspAP) | `http://10.3.141.1` | admin / secret |
| **File Manager** | `http://10.3.141.1:8080` | admin / (random, define via script) |
| **Network Share** | `\\10.3.141.1\share` | (User defined via script) |
| **SSH** | `10.3.141.1` | user / UserPass |

---

## 📜 Script Reference

All scripts are located in `scripts/`. Always run from the project root or scripts directory.

### Setup & Installation
- `install_base.sh`: Updates OS and installs common utilities.
- `install_docker.sh`: Installs Docker and Compose.
- `install_wifi_driver.sh`: Compiles and installs Realtek Wi-Fi drivers.
- `install_mnt_fstab.sh`: Configures `/etc/fstab` to auto-mount USB storage to `/mnt/ssd`.

### Service Management
- `create_fileserver.sh`: Pulls images and starts the Docker containers (First run).
- `start_fileserver.sh`: Starts existing containers.
- `stop_fileserver.sh`: Stops containers gracefully.
- `manage_samba_user.sh`: Adds/modifies/deletes Samba users. command: `add`, `password`, `delete`, `list`.
- `reset_filebrowser_password.sh`: Resets the `admin` password for the web file manager.

### Network & Maintenance
- `full_network_reset.sh`: **Emergency Only**. Resets RaspAP configuration, networking, hostapd, anddnsmasq to defaults. Useful if you lock yourself out of the hotspot.
- `firewall_strict.sh`: Applies strict iptables rules (blocks WAN access for clients).
- `firewall_maintenance.sh`: Opens firewall for updates/maintenance.
- `diagnose_raspap.sh`: Checks status of critical network services.

---

## 🐛 Troubleshooting

**"I can't see files in the share"**
If using a Mac, the "fruit" extensions for Samba might conflict with exFAT.
Run: `sudo docker compose -f docker/docker-compose.yml restart samba`
(The config already disables fruit extensions by default).

**"Web UI is slow"**
Check if `UseDNS` is disabled in `/etc/ssh/sshd_config` (handled by `full_network_reset.sh`).

**"FileBrowser database locked"**
Use `./scripts/stop_fileserver.sh` then start again. The reset password script handles locking automatically.