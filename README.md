# Solar Panel Test Bench

A programmable LED illumination bench for testing solar panels used in low-power embedded systems under controlled, repeatable lighting conditions.

## Features

- **Eight LED channels**, driven by PWM at **259 kHz**. The channels are evenly phase-shifted by **1/8 of a period** to minimize fluctuations in the combined light output.
- **Programmable lighting scenarios** to reproduce varying illumination conditions.
- **Wi-Fi connectivity** using a Raspberry Pi Pico W.

## Repository structure

- `board/`: KiCad files for the main PCB.
- `firmware/`: Arduino firmware for the Pico W. See the `.ino` file for dependencies and setup instructions.
