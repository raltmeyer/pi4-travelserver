#!/bin/bash

echo "=== RaspAP Diagnostic Report ==="
echo "Generated on: $(date)"
echo ""

echo "--- System Status ---"
uptime
echo ""

echo "--- CPU Temperature ---"
if [ -f /sys/class/thermal/thermal_zone0/temp ]; then
    cpu_temp=$(cat /sys/class/thermal/thermal_zone0/temp)
    echo "CPU Temp: $((cpu_temp/1000))'C"
else
    echo "CPU Temp: Cannot read"
fi
echo ""

echo "--- Memory Usage ---"
free -h
echo ""

echo "--- Disk Usage ---"
df -h /
echo ""

echo "--- Top 5 CPU Processes ---"
ps -eo pid,ppid,cmd,%mem,%cpu --sort=-%cpu | head -n 6
echo ""

echo "--- Network Connectivity (Ping) ---"
echo "Pinging 8.8.8.8 (Google DNS)..."
ping -c 3 -W 2 8.8.8.8
echo ""

echo "--- DNS Resolution Speed ---"
echo "Resolving google.com..."
time nslookup google.com 8.8.8.8
echo ""
echo "Resolving google.com (Local DNS)..."
time nslookup google.com
echo ""

echo "--- Service Status ---"
systemctl status hostapd --no-pager | head -n 10
echo ""
systemctl status dnsmasq --no-pager | head -n 10
echo ""
systemctl status lighttpd --no-pager | head -n 10
echo ""

echo "--- Recent Syslog Errors ---"
grep -i "error" /var/log/syslog | tail -n 10
echo ""

echo "=== End of Report ==="
