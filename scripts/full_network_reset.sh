#!/bin/bash

# Configuration
AP_IFACE="wlan0"
CLIENT_IFACE="wlan1"
AP_IP="10.3.141.1"
AP_SSID="TravelPi_5G"
AP_PASS="ChangeMe123"

# Ensure run as root
if [ "$EUID" -ne 0 ]; then
  echo "Please run as root (sudo ./full_network_reset.sh)"
  exit 1
fi

echo "=========================================="
echo "    Travel Server Full Network Reset      "
echo "=========================================="
echo "This script will completely wipe and restore network settings."
echo "Target Configuration:"
echo "  - AP (Hotspot): $AP_IFACE ($AP_IP)"
echo "  - Client (WAN): $CLIENT_IFACE (DHCP)"
echo "  - Ethernet: eth0 (DHCP)"
echo "------------------------------------------"

# 1. Stop Everything
echo "[1/8] Stopping services..."
systemctl stop hostapd
systemctl stop dnsmasq
systemctl stop dhcpcd
systemctl stop wpa_supplicant
killall wpa_supplicant 2>/dev/null
killall hostapd 2>/dev/null
killall dnsmasq 2>/dev/null

# 2. Flush IP Addresses
echo "[2/8] Flushing interfaces..."
ip addr flush dev $AP_IFACE
ip addr flush dev $CLIENT_IFACE
ip link set $AP_IFACE down
ip link set $CLIENT_IFACE down

# 3. Restore wpa_supplicant Service Fix
echo "[3/8] Installing wpa_supplicant fix for $CLIENT_IFACE..."
cat > /etc/systemd/system/wpa_supplicant-wlan1.service <<EOF
[Unit]
Description=WPA supplicant daemon ($CLIENT_IFACE specific)
Before=network.target
After=dbus.service
Wants=network.target

[Service]
ExecStart=/sbin/wpa_supplicant -u -s -O /run/wpa_supplicant -i $CLIENT_IFACE -c /etc/wpa_supplicant/wpa_supplicant.conf -D nl80211,wext
Restart=on-failure
Alias=dbus-fi.w1.wpa_supplicant1.service

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
# Disable generic wpa_supplicant to avoid conflicts
systemctl disable wpa_supplicant
systemctl enable wpa_supplicant-wlan1.service

# 4. Reset dhcpcd.conf
echo "[4/8] Resetting /etc/dhcpcd.conf..."
# Backup if not exists
if [ ! -f /etc/dhcpcd.conf.bak ]; then
    cp /etc/dhcpcd.conf /etc/dhcpcd.conf.bak
fi

cat > /etc/dhcpcd.conf <<EOF
hostname
clientid
persistent
option rapid_commit
option domain_name_servers, domain_name, domain_search, host_name
option classless_static_routes
option interface_mtu
require dhcp_server_identifier
slaac private

# Eth0 (DHCP) - Standard Wired Connection
interface eth0

# AP Interface (Static IP for Hotspot)
interface $AP_IFACE
    static ip_address=$AP_IP/24
    nohook wpa_supplicant

# Client Interface (DHCP for Hotel WiFi)
interface $CLIENT_IFACE
EOF

# 5. Reset hostapd.conf
echo "[5/8] Resetting /etc/hostapd/hostapd.conf..."
cat > /etc/hostapd/hostapd.conf <<EOF
interface=$AP_IFACE
driver=nl80211
ssid=$AP_SSID
hw_mode=a
channel=36
ieee80211d=1
country_code=US
ieee80211n=1
ieee80211ac=1
wmm_enabled=1

auth_algs=1
wpa=2
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
wpa_passphrase=$AP_PASS
EOF

# 6. Reset dnsmasq.conf
echo "[6/8] Resetting /etc/dnsmasq.conf..."
cat > /etc/dnsmasq.conf <<EOF
interface=$AP_IFACE
dhcp-range=10.3.141.50,10.3.141.255,255.255.255.0,24h
domain=wlan
address=/gw.wlan/$AP_IP
EOF

# 7. Configure NAT & IPTables
echo "[7/8] Configuring NAT..."
sh -c "echo 1 > /proc/sys/net/ipv4/ip_forward"
sed -i 's/#net.ipv4.ip_forward=1/net.ipv4.ip_forward=1/' /etc/sysctl.conf

# Flush old rules
iptables -t nat -F
iptables -F
# Add new NAT rules (Masquerade for both wlan1 and eth0 out)
iptables -t nat -A POSTROUTING -o $CLIENT_IFACE -j MASQUERADE
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
# Allow forwarding
iptables -A FORWARD -i $CLIENT_IFACE -o $AP_IFACE -m state --state RELATED,ESTABLISHED -j ACCEPT
iptables -A FORWARD -i $AP_IFACE -o $CLIENT_IFACE -j ACCEPT
# Save
netfilter-persistent save 2>/dev/null || echo "Warning: netfilter-persistent not found, rules not saved permanently."

# 8. Restart Everything
echo "[8/8] Starting services..."
systemctl unmask hostapd
systemctl enable hostapd
# Start in order
ip link set $AP_IFACE up
ip link set $CLIENT_IFACE up
systemctl restart dhcpcd
systemctl restart wpa_supplicant-wlan1.service
systemctl restart dnsmasq
systemctl restart hostapd

echo "=========================================="
echo "             Reset Complete               "
echo "=========================================="
echo "AP SSID: $AP_SSID"
echo "Check connection in 30 seconds."

bash firewall_maintenance.sh
reboot