# bootm03x

Bootloader for Nuvoton M03x microcontrollers (M031 and M032).

* [Nuvoton M031 Series BSP](https://github.com/OpenNuvoton/M031BSP/)

* [NuEclipse IDE](https://www.nuvoton.com/tool-and-software/ide-and-compiler/) 

## Usage

This program is intended to be compiled with linker chages to be placed at LDROM memory (address starting from 0x100000).

It also definies initial data for User Config Data (3x 32 bits words starting from 0x300000).

This project must compiled as a static library which needs to be linked in the application project. The initial configuration data defines that the application will boot from **APROM with IAP** source as default. When program data is received via UART protocol and placed at transfer region, the application needs to set boot source to **LDROM with IAP**.

## Operation

When booting from LDROM, this bootloader program performs the operation described in the char below:

![Bootloader operation flowchart.](flowchart.png "Bootloader operation flowchart.")

## Changelog

### v2.0
* Initial version.
