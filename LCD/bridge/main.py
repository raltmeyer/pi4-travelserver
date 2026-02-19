import time
import json
import psutil
import serial
import serial.tools.list_ports
import threading
import os
import subprocess
import socket

# Configuration
SERIAL_BAUDRATE = 115200
UPDATE_INTERVAL = 2  # Seconds

class SystemMonitor:
    def get_cpu_usage(self):
        return psutil.cpu_percent(interval=None)

    def get_ram_usage(self):
        ram = psutil.virtual_memory()
        return {
            "total": ram.total // (1024 * 1024),
            "used": ram.used // (1024 * 1024),
            "percent": ram.percent
        }

    def get_disk_usage(self):
        disk = psutil.disk_usage('/')
        return {
            "total": disk.total // (1024 * 1024 * 1024),
            "used": disk.used // (1024 * 1024 * 1024),
            "percent": disk.percent
        }

    def get_temperature(self):
        try:
            temps = psutil.sensors_temperatures()
            if 'cpu_thermal' in temps:
                return temps['cpu_thermal'][0].current
            elif 'coretemp' in temps:
                return temps['coretemp'][0].current
            return 0
        except:
            return 0

    def get_ip_address(self, interface):
        try:
            return net_if_addrs()[interface][0].address
        except:
            return "N/A"

    def get_network_info(self):
        net_info = {}
        interfaces = psutil.net_if_addrs()
        
        # Customize based on your specific interfaces (wlan0, wlan1, eth0, usb0)
        for iface in ['wlan0', 'wlan1', 'eth0', 'usb0']:
            if iface in interfaces:
                for addr in interfaces[iface]:
                    if addr.family == socket.AF_INET:
                        net_info[iface] = addr.address
        return net_info
        
    def get_uptime(self):
        return int(time.time() - psutil.boot_time())

class SerialBridge:
    def __init__(self):
        self.ser = None
        self.monitor = SystemMonitor()
        self.running = True

    def find_esp32(self):
        ports = list(serial.tools.list_ports.comports())
        for port in ports:
            # Common ESP32 USB-Serial descriptions/VIDs
            if "CP210" in port.description or "CH340" in port.description or "USB Serial" in port.description:
                return port.device
        # Fallback to standard linux locations if auto-detection fails
        if os.path.exists('/dev/ttyUSB0'): return '/dev/ttyUSB0'
        if os.path.exists('/dev/ttyACM0'): return '/dev/ttyACM0'
        return None

    def connect(self):
        while self.running:
            port = self.find_esp32()
            if port:
                try:
                    self.ser = serial.Serial(port, SERIAL_BAUDRATE, timeout=1, rtscts=False, dsrdtr=False)
                    self.ser.dtr = False
                    self.ser.rts = False
                    print(f"Connected to ESP32 on {port}")
                    self.ser.reset_input_buffer()
                    return True
                except Exception as e:
                    print(f"Failed to connect: {e}")
            else:
                print("ESP32 not found. Retrying...")
            time.sleep(5)

    def read_loop(self):
        print("Read loop started")
        while self.running:
            if self.ser and self.ser.is_open:
                try:
                    if self.ser.in_waiting > 0:
                        line = self.ser.readline().decode('utf-8').strip()
                        if line:
                            # Debug: print all lines
                            print(f"[RAW] {line}")
                            self.handle_command(line)
                except Exception as e:
                    print(f"Read error: {e}")
                    self.ser.close()
                    self.connect()
            time.sleep(0.1)

    def handle_command(self, data):
        try:
            cmd = json.loads(data)
            print(f"Received command: {cmd}")
            
            if cmd.get("action") == "reboot":
                subprocess.run(["sudo", "reboot"])
            elif cmd.get("action") == "shutdown":
                subprocess.run(["sudo", "shutdown", "-h", "now"])
            elif cmd.get("action") == "reset_network":
                subprocess.run(["sudo", "/home/raltmeyer/pi4-travelserver/scripts/full_network_reset.sh"])
            elif cmd.get("action") == "fw_strict":
                subprocess.run(["sudo", "/home/raltmeyer/pi4-travelserver/scripts/firewall_strict.sh"])
            elif cmd.get("action") == "fw_maint":
                subprocess.run(["sudo", "/home/raltmeyer/pi4-travelserver/scripts/firewall_maintenance.sh"])
            elif cmd.get("action") == "start_smb":
                subprocess.run(["sudo", "/home/raltmeyer/pi4-travelserver/scripts/start_fileserver.sh"])
            elif cmd.get("action") == "stop_smb":
                subprocess.run(["sudo", "/home/raltmeyer/pi4-travelserver/scripts/stop_fileserver.sh"])
            
        except json.JSONDecodeError:
            print(f"Invalid JSON received: {data}")

    def write_loop(self):
        while self.running:
            if self.ser and self.ser.is_open:
                try:
                    stats = {
                        "cpu": self.monitor.get_cpu_usage(),
                        "ram": self.monitor.get_ram_usage(),
                        "disk": self.monitor.get_disk_usage(),
                        "temp": self.monitor.get_temperature(),
                        "net": self.monitor.get_network_info(),
                        "uptime": self.monitor.get_uptime()
                    }
                    json_stats = json.dumps(stats)
                    self.ser.write((json_stats + '\n').encode('utf-8'))
                except Exception as e:
                    print(f"Write error: {e}")
            time.sleep(UPDATE_INTERVAL)

    def start(self):
        self.connect()
        
        read_thread = threading.Thread(target=self.read_loop)
        read_thread.daemon = True
        read_thread.start()
        
        self.write_loop()

if __name__ == "__main__":
    bridge = SerialBridge()
    try:
        bridge.start()
    except KeyboardInterrupt:
        print("Stopping bridge...")
        bridge.running = False
