# LCD Display Setup & Deployment

The Travel Server includes a smart LCD display (ESP32-S3) that shows network status and allows basic configuration.

## ⚠️ Important: Local Deployment Required

**You cannot deploy the LCD code from the Raspberry Pi.**

The firmware must be compiled and uploaded from your **local computer** (macOS, Windows, or Linux) because:
1.  **PlatformIO** (the build tool) requires significant resources.
2.  The ESP32-S3 must be connected via USB to the machine running the deployment command.

## Prerequisites (Local Machine)

1.  **Python 3** installed.
2.  **PlatformIO Core** installed via pip.

### Installation Steps (macOS/Linux)

1.  **Install PlatformIO Core:**
    ```bash
    pip3 install -U platformio
    ```

2.  **Fix Path Issue (Critical):**
    The installation often places the `pio` command in a user directory that isn't in your system PATH.
    
    *Temporary fix (run in your current terminal):*
    ```bash
    export PATH=$PATH:$(python3 -m site --user-base)/bin
    ```
    
    *Permanent fix:* Add the line above to your `~/.zshrc` or `~/.bash_profile`.

3.  **Verify Installation:**
    ```bash
    pio --version
    ```

## Firmware Build & Upload

1.  **Connect the Hardware:**
    Connect the LCD (ESP32-S3) to your computer via USB. Use the port marked "USB" or "OTG".

2.  **Navigate to Firmware Directory:**
    ```bash
    cd LCD/firmware
    ```

3.  **Build and Upload:**
    This command compiles the code and attempts to flash it to the device automatically.
    ```bash
    pio run -t upload
    ```

## Troubleshooting

-   **"pio: command not found"**:
    You skipped Step 2 in "Installation Steps". Run the `export PATH...` command.

-   **"Fatal error: This chip is ESP32, not ESP32-S3"**:
    -   **Cause:** You have a standard ESP32 board, but `platformio.ini` is set for S3.
    -   **Fix:** Change `board = esp32-s3-devkitc-1` to `board = esp32dev` in `LCD/firmware/platformio.ini`.

-   **"Auto-detected: /dev/cu.Bluetooth..." (Wrong Port)**:
    Your computer isn't seeing the ESP32.
    1.  **Check Cable:** Ensure it's a **Data Cable**.
    2.  **Bootloader Mode:** Hold the **BOOT** button on the device, press **RESET**, then release BOOT.

-   **Device not found / Board doesn't power on:**
    -   **Mac USB-C Issue:** Many ESP32 boards (like the JC2432W328) fail to negotiate power with a direct USB-C to USB-C cable.
    -   **Fix:** Use a **USB-A to USB-C cable** (rectangular plug to oval plug), or connect through a **USB Hub**. This forces 5V power.
-   **Upload failed:** Put the device in bootloader mode (Hold BOOT, press RESET, release BOOT).

-   **Permission Denied:**
    You might need to run the upload command with `sudo` (rare on macOS, common on Linux).

---

## Hardware Pinout — ESP32-2432S028

> **Board:** AITEXM ROBOT 2.8" LCD TFT with Touch (ESP32-WROOM, ST7789 driver, XPT2046 touch)

### ⚠️ Critical: Touch Uses a Separate SPI Bus

The TFT display and the touch controller are on **two independent SPI buses**. Most CYD tutorials assume they share the same bus — this is **wrong** for this board variant.

### Pin Map

| Function | TFT Display (HSPI) | Touch Controller (VSPI) |
|----------|-------------------|------------------------|
| CLK      | GPIO 14           | GPIO 25                |
| MOSI     | GPIO 13           | GPIO 32 (TP DIN)       |
| MISO     | GPIO 12           | GPIO 39 (TP OUT)       |
| CS       | GPIO 15           | GPIO 33                |
| IRQ      | —                 | GPIO 36                |
| DC       | GPIO 2            | —                      |
| Backlight| GPIO 21 (HIGH=on) | —                      |
| RST      | -1 (not connected)| —                      |

### Other Pins

| Function         | Pin(s)         |
|------------------|----------------|
| Audio             | GPIO 26        |
| RGB LED           | GPIO 17, 4, 16 |
| SD Card (TF)      | CLK=18, CMD=23, DAT0=19, CD=5 |
| I2C Expand (IO1)  | SCL=22, SDA=21 |
| I2C Expand (IO2)  | SCL=22         |

## Touch Configuration

### Libraries
- **TFT_eSPI** for display (all pin config via `build_flags` in `platformio.ini`)
- **Raw SPI** for touch — no separate touch library needed; just use `SPIClass(VSPI)`

### Initialization
```cpp
SPIClass touchSPI(VSPI);
touchSPI.begin(25, 39, 32, 33);  // CLK, MISO, MOSI, CS
```

### Axis Mapping (Landscape, rotation=1)
The XPT2046 axes are swapped relative to the display:
```cpp
int screenX = map(rawY, 200, 3800, 0, 320);  // X from rawY
int screenY = map(rawX, 300, 3700, 0, 240);  // Y from rawX
```

### PENIRQ Power-Down (Required!)
After every touch read cycle, you **must** send a power-down command to re-enable the IRQ pin — otherwise touch only works once:
```cpp
void touchPowerDown() {
  touchSPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(TP_CS, LOW);
  touchSPI.transfer(0x80);  // PD=00: power down + enable PENIRQ
  touchSPI.transfer(0);
  touchSPI.transfer(0);
  digitalWrite(TP_CS, HIGH);
  touchSPI.endTransaction();
}
```

