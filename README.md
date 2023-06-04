# Sonix Flasher C

A CLI-based Flasher for Sonix 24x/26x MCUs.

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


## License

This project is licensed under the GNU License - see the LICENSE.md file for details
