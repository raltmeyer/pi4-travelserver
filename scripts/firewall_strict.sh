#!/bin/bash

# Ensure run as root
if [ "$EUID" -ne 0 ]; then
  echo "Please run as root (sudo ./firewall_strict.sh)"
  exit 1
fi

echo "APplying STRICT Firewall Rules..."
echo " - BLOCKING input on eth0 (Wired) and wlan1 (Hotel)"
echo " - ALLOWING input on wlan0 (Internal AP)"
echo " - ALLOWING Internet Sharing (NAT)"

# 1. IP Forwarding (Ensure it's on)
echo 1 > /proc/sys/net/ipv4/ip_forward

# 2. Flush Existing Rules
iptables -F
iptables -X
iptables -t nat -F
iptables -t nat -X

# 3. Default Policies (Drop everything by default)
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -P OUTPUT ACCEPT

# 4. Allow Loopback
iptables -A INPUT -i lo -j ACCEPT

# 5. Allow Established/Related Connections
# This ensures your current SSH session (and return traffic from internet) is allowed.
iptables -A INPUT -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
iptables -A FORWARD -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT

# 6. Allow Internal AP (wlan0) - Trusted Zone
# Devices connected to the hotspot can access the Pi and the Internet.
iptables -A INPUT -i wlan0 -j ACCEPT
iptables -A FORWARD -i wlan0 -j ACCEPT

# 7. Enable NAT (Masquerade) for Internet Sharing
# Allow traffic going out via wlan1 (WiFi) or eth0 (Wired)
iptables -t nat -A POSTROUTING -o wlan1 -j MASQUERADE
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE

# 8. Save Rules
netfilter-persistent save

echo "----------------------------------------"
echo "STRICT Mode Active."
echo "Only devices on 'TravelPi_5G' can access this server."
echo "Wired/Hotel ports are CLOSED."
