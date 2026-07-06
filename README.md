# NKE for ESP32-C3

This branch records the port of **NKE (Nano Kernel Educational)** to the **ESP32-C3**.

This work was developed by our colleague **GabrielPCamargo** and its main reference is the repository:

- [GabrielPCamargo/nke_esp32c3](https://github.com/GabrielPCamargo/nke_esp32c3)

This README only exists to place this branch in the context of the **Real-Time Systems** course repository from the **Computer Engineering program at UERGS**. Detailed explanations about the port, the original project organization, implementation decisions, and complete instructions should be consulted in Gabriel's repository README.

## About This Branch

Unlike the previous branches in this repository, this version is **no longer based on NKE Arduino**.

The code is organized as a C implementation for the ESP32-C3, using the bare-metal build structure in `mdk/` and the main code in `src/NKE.c`.

In practical terms, this branch represents a change of target:

- the Arduino/AVR environment used in the first NKE adaptations is no longer the target;
- the ESP32-C3 becomes the execution platform;
- the focus is the NKE port to this architecture, based on Gabriel's project.

## Basic Structure

- `src/NKE.c`: main NKE code ported to ESP32-C3.
- `src/Makefile`: build configuration for the `esp32c3` target.
- `build.sh`: helper script to build from the repository root.
- `mdk/`: bare-metal structure used for ESP32/ESP32-C3 build and support.
- `include/`: auxiliary definitions used by the port.

## Build

From the repository root:

```sh
./build.sh
```

For other `Makefile` targets, pass the target to the script:

```sh
./build.sh clean
./build.sh flash
./build.sh monitor
```

By default, the script uses:

- `ARCH=esp32c3`
- `PORT=/dev/ttyACM0`

If needed, adjust these settings according to the board and serial port being used.

## Main Reference

To understand the project in detail, consult:

- [GabrielPCamargo/nke_esp32c3](https://github.com/GabrielPCamargo/nke_esp32c3)

## Institution

**Universidade Estadual do Rio Grande do Sul (UERGS)**  
Course: **Computer Engineering**  
Discipline: **Real-Time Systems**
