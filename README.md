# Sonix Flasher C

A CLI-based Flasher for Sonix SN32F2xx MCUs.

## Description

This flasher is aimed for advanced users that don't need a GUI. If you require a GUI please use: [Sonix-Flasher](https://github.com/SonixQMK/sonix-flasher).

Features:

- [x] Minimal dependencies
- [x] Cross-platform
- [x] Faster

## Getting Started

### Dependencies

Clone this repository:
```
git clone https://github.com/SonixQMK/SonixFlasherC
```

[hidapi](https://github.com/libusb/hidapi) is a prequisite.
If using windows, please install MinGW

### Compiling

Compile using:

```
make sonixflasher
```


### Running the flasher

```
./sonixflasher
```

#### Command List:

- `--vidpid -v`      Set VID and PID for the device to flash.
- `--offset -o`      Set flashing offset (default: 0).
- `--file -f`        Binary of the firmware to flash (*.bin extension).
- `--jumploader -j`  Define if flashing a jumploader.
- `--reboot -r`      Request bootloader reboot in OEM firmware (options: sonix, evision, hfd).
- `--debug -d`       Enable debug mode.
- `--list-vidpid -l` Display supported VID/PID pairs.
- `--nooffset -k`    Disable offset checks.
- `--version -V`     Print version information.
- `--help -h`        Show this help message.

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

Notice that some devices support flashing while booted. In that case, use
```
--reboot
```
to expose the ISP mode

## Usage Examples

- **Flash jumploader to device with VID/PID 0x0c45/0x7040:**

  ```
  sonixflasher --vidpid 0c45/7040 --file fw.bin -j
  ```
- **Flash firmware to device with VID/PID 0x0c45/0x7040 and offset 0x200:**

  ```
  sonixflasher --vidpid 0c45/7040 --file fw.bin -o 0x200
  ```

## License

This project is licensed under the GNU License - see the LICENSE.md file for details
