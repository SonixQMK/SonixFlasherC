# Sonix Flasher C

A CLI-based flasher and diagnostics tool for Sonix SN32F2xx USB bootloader devices.

## Description

This project is intended for advanced users who want a lightweight, dependency-minimal command-line tool without a GUI.

Features:

- [x] HID-based USB flashing
- [x] Firmware and jumploader programming
- [x] Reboot to bootloader from OEM firmware via supported reboot types
- [x] Reboot to user mode from bootloader
- [x] Chip identification
- [x] Device info reporting: chip family, memory size, security level, checksum
- [x] Minimal dependencies
- [x] Cross-platform
- [x] Faster

## Getting Started

### Dependencies

Clone this repository:

```bash
git clone https://github.com/SonixQMK/SonixFlasherC
```

The default USB backend is `libusb`.

The project also supports `hidapi` for legacy compatibility. The backend can be selected when building with the BACKEND Make variable.

You will need the development package for the selected backend and pkg-config.

### Compiling

Build using the default libusb backend:

```bash
make
```

or:
```bash
make BACKEND=libusb
```

To build using the legacy hidapi backend:

```bash
make BACKEND=hidapi
```

### Firmware File Format

The flasher currently accepts **raw binary (`.bin`) firmware images only**.

Intel HEX (`.hex`) files are not supported directly. Convert them to a raw `.bin` file before flashing.

### Running the flasher

```bash
./sonixflasher [OPTIONS]
```

Firmware and jumploader files must be provided as `.bin` files.

### Supported options

- `-v, --vid-pid VID/PID`    Device VID/PID in `XXXX/XXXX` format (required unless using a list-only command)
- `-f, --file PATH`          Firmware `.bin` file path (required for flashing)
- `-o, --offset ADDR`        Flash offset (default: `0`)
- `-j, --jumploader`         Flash a jumploader `.bin` image instead of a normal firmware image
- `-r, --reboot TYPE`        Request bootloader reboot before flashing (`sonix`, `evision`, or `hfd`)
- `-k, --no-offset-check`    Skip offset validation for F26X flows
- `-u, --user-mode`          Reboot a device back to user mode from bootloader state (no file required)
- `-i, --info`               Print chip/device info and flash checksum (no file required)
- `-d, --debug`              Enable debug output
- `-l, --list-devices`       List all supported devices and their VID/PID pairs
- `-c, --list-connected`     Scan and list connected supported devices
- `-V, --version`            Print version information
- `-h, --help`               Show command help

#### ISP Bootloader Mode Defaults:

|      Device     |   VID  |   PID  |
|-----------------|--------|--------|
| SONIX SN32F22x  | 0x0C45 | 0x7900 |
| SONIX SN32F23x  | 0x0C45 | 0x7900 |
| SONIX SN32F24x  | 0x0C45 | 0x7900 |
| SONIX SN32F24xB | 0x0C45 | 0x7040 |
| SONIX SN32F24xC | 0x0C45 | 0x7160 |
| SONIX SN32F26x  | 0x0C45 | 0x7010 |
| SONIX SN32F28x  | 0x0C45 | 0x7120 |
| SONIX SN32F29x  | 0x0C45 | 0x7140 |

Notice that some devices support flashing while running their OEM firmware. In those cases, use `--reboot` to expose the ISP mode.

## Usage Examples

### Flash a normal firmware image

```bash
./sonixflasher -v 0c45/7040 -f firmware.bin
```

### Flash a jumploader image

```bash
./sonixflasher -v 0c45/7040 -f bootloader.bin -j
```

### Flash with an offset

```bash
./sonixflasher -v 0c45/7040 -f firmware.bin -o 0x200
```

### Request a reboot to bootloader before flashing

```bash
./sonixflasher -v 0c45/7040 -f firmware.bin -r sonix
```

### Reboot a device back to user mode

```bash
./sonixflasher -v 0c45/7040 -u
```

### Print device info and checksum

```bash
./sonixflasher -v 0c45/7040 -i
```

### List all supported devices

```bash
./sonixflasher -l
```

### List currently connected supported devices

```bash
./sonixflasher -c
```

## License

This project is licensed under the GNU License - see the LICENSE.md file for details
