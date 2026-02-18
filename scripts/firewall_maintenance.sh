#!/bin/bash

# Ensure run as root
if [ "$EUID" -ne 0 ]; then
  echo "Please run as root (sudo ./firewall_maintenance.sh)"
  exit 1
fi

echo "Applying MAINTENANCE Firewall Rules..."
echo " - OPENING Management Ports on eth0 (Wired)"
echo " - BLOCKING input on wlan1 (Hotel)"
echo " - ALLOWING input on wlan0 (Internal AP)"

# 1. IP Forwarding
echo 1 > /proc/sys/net/ipv4/ip_forward

# 2. Flush
iptables -F
iptables -X
iptables -t nat -F
iptables -t nat -X

# 3. Default Policies
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -P OUTPUT ACCEPT

# 4. Allow Loopback
iptables -A INPUT -i lo -j ACCEPT

# 5. Allow Established/Related
iptables -A INPUT -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
iptables -A FORWARD -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT

# 6. Allow Internal AP (wlan0)
iptables -A INPUT -i wlan0 -j ACCEPT
iptables -A FORWARD -i wlan0 -j ACCEPT

# 7. Allow Maintenance on eth0 (Wired Only)
# Open SSH, Web UI, SMB, etc.
iptables -A INPUT -i eth0 -p tcp --dport 22 -j ACCEPT   # SSH
iptables -A INPUT -i eth0 -p tcp --dport 80 -j ACCEPT   # HTTP (RaspAP)
iptables -A INPUT -i eth0 -p tcp --dport 8080 -j ACCEPT # FileBrowser
iptables -A INPUT -i eth0 -p tcp --dport 139 -j ACCEPT  # SMB
iptables -A INPUT -i eth0 -p tcp --dport 445 -j ACCEPT  # SMB
iptables -A INPUT -i eth0 -p udp --dport 137 -j ACCEPT  # NetBIOS
iptables -A INPUT -i eth0 -p udp --dport 138 -j ACCEPT  # NetBIOS

# 8. Enable NAT
iptables -t nat -A POSTROUTING -o wlan1 -j MASQUERADE
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE

# 9. Save
netfilter-persistent save

echo "----------------------------------------"
echo "MAINTENANCE Mode Active."
echo "You can connect via Ethernet (eth0) to manage the server."
echo "Hotel WiFi input remains blocked."
