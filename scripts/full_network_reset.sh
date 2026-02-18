#!/bin/bash

# ==========================================
# RaspAP Compliant Network Reset Script
# ==========================================
# This script resets the Raspberry Pi's network configuration 
# to the standard RaspAP defaults.
#
# IMPACT:
# - Resets /etc/dhcpcd.conf
# - Resets /etc/dnsmasq.conf
# - Resets /etc/hostapd/hostapd.conf
# - Resets /etc/raspap/hostapd.ini (Web UI Settings)
# - Fixes SSH login speed (UseDNS)
#
# USAGE: sudo ./full_network_reset.sh
# ==========================================

# Configuration Defaults (RaspAP Standard)
AP_IFACE="wlan0"
CLIENT_IFACE="wlan1"
AP_IP="10.3.141.1"
AP_SSID="raspi-webgui"
AP_PASS="ChangeMe"
AP_CHANNEL="149" # 5GHz UNII-3 (High band) to avoid interference with Ch 36-48 uplink
COUNTRY="US"

# Ensure run as root
if [ "$EUID" -ne 0 ]; then
  echo "Please run as root (sudo ./full_network_reset.sh)"
  exit 1
fi

echo "=========================================="
echo "    RaspAP Full Network Reset (Compliant) "
echo "=========================================="
echo "Target Configuration:"
echo "  - AP Interface: $AP_IFACE ($AP_IP)"
echo "  - SSID: $AP_SSID"
echo "  - Web UI: Synced with backend"
echo "------------------------------------------"

# 1. Stop Services
echo "[1/7] Stopping services..."
systemctl stop hostapd
systemctl stop dnsmasq
systemctl stop dhcpcd
systemctl stop lighttpd

# 2. Reset /etc/dhcpcd.conf
echo "[2/7] Resetting /etc/dhcpcd.conf..."
cp /etc/dhcpcd.conf /etc/dhcpcd.conf.bak.$(date +%s) 2>/dev/null
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

# 3. Reset /etc/dnsmasq.conf
echo "[3/7] Resetting /etc/dnsmasq.conf..."
cp /etc/dnsmasq.conf /etc/dnsmasq.conf.bak.$(date +%s) 2>/dev/null
cat > /etc/dnsmasq.conf <<EOF
interface=$AP_IFACE
dhcp-range=10.3.141.50,10.3.141.255,255.255.255.0,24h
domain=wlan
address=/gw.wlan/$AP_IP
EOF

# 4. Reset /etc/hostapd/hostapd.conf
echo "[4/7] Resetting /etc/hostapd/hostapd.conf..."
cp /etc/hostapd/hostapd.conf /etc/hostapd/hostapd.conf.bak.$(date +%s) 2>/dev/null
cat > /etc/hostapd/hostapd.conf <<EOF
interface=$AP_IFACE
driver=nl80211
ssid=$AP_SSID
hw_mode=a
channel=$AP_CHANNEL
ieee80211d=1
country_code=$COUNTRY
ieee80211n=1
ieee80211ac=1
wmm_enabled=1

# High Performance Settings (Pi 4 BCM43455)
# Verified from 'iw phy' output - only use supported capabilities
ht_capab=[HT40+][SHORT-GI-20][SHORT-GI-40]
vht_capab=[SHORT-GI-80]
vht_oper_chwidth=1
vht_oper_centr_freq_seg0_idx=155

auth_algs=1
wpa=2
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
wpa_passphrase=$AP_PASS
EOF

# 5. Sync Web UI Settings (/etc/raspap/hostapd.ini)
# This is CRITICAL for RaspAP compliance. The Web UI reads this file.
echo "[5/7] Syncing RaspAP Web UI settings..."
if [ -f /etc/raspap/hostapd.ini ]; then
    cp /etc/raspap/hostapd.ini /etc/raspap/hostapd.ini.bak.$(date +%s)
    
    # We use sed to update the existing INI file preservation of other settings
    # or write a fresh basic one if preferred. Updating is safer.
    
    # Init basic file if empty/missing (Unlikely but safe)
    if [ ! -s /etc/raspap/hostapd.ini ]; then
        echo "Creating fresh hostapd.ini..."
        cat > /etc/raspap/hostapd.ini <<EOF
WifiInterface = $AP_IFACE
SecurityType = wpa2
SSID = $AP_SSID
wpa_passphrase = $AP_PASS
Channel = $AP_CHANNEL
EOF
    else
        # Update existing values using sed
        sed -i "s/^WifiInterface.*/WifiInterface = $AP_IFACE/" /etc/raspap/hostapd.ini
        sed -i "s/^SSID.*/SSID = $AP_SSID/" /etc/raspap/hostapd.ini
        sed -i "s/^wpa_passphrase.*/wpa_passphrase = $AP_PASS/" /etc/raspap/hostapd.ini
        sed -i "s/^Channel.*/Channel = $AP_CHANNEL/" /etc/raspap/hostapd.ini
        # Ensure Managed Mode is correct
        sed -i "s/^WifiManaged.*/WifiManaged = $CLIENT_IFACE/" /etc/raspap/hostapd.ini
    fi
    
    # Ensure ownership (usually www-data)
    chown www-data:www-data /etc/raspap/hostapd.ini
else
    echo "Warning: /etc/raspap/hostapd.ini not found. Is RaspAP installed?"
fi

# 6. Fix SSH Login Speed
echo "[6/7] Fixing SSH login speed (UseDNS)..."
if grep -q "^#UseDNS" /etc/ssh/sshd_config || grep -q "^UseDNS" /etc/ssh/sshd_config; then
    sed -i 's/^#UseDNS.*/UseDNS no/' /etc/ssh/sshd_config
    sed -i 's/^UseDNS yes/UseDNS no/' /etc/ssh/sshd_config
else
    echo "UseDNS no" >> /etc/ssh/sshd_config
fi
systemctl restart ssh 2>/dev/null || systemctl restart sshd 2>/dev/null

# 7. Restart Services
echo "[7/7] Restarting services..."
systemctl unmask hostapd
systemctl enable hostapd
systemctl restart dhcpcd
systemctl restart dnsmasq
systemctl restart hostapd
systemctl restart lighttpd

echo "=========================================="
echo "             Reset Complete               "
echo "=========================================="
echo "AP SSID: $AP_SSID"
echo "AP Pass: $AP_PASS"
echo "Gateway: $AP_IP"
echo "Web UI:  http://$AP_IP"
echo "=========================================="