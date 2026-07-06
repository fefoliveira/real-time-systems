# NKE ESP32-C3 Timer Counter

This branch records an implementation built on top of the base **NKE (Nano Kernel Educational)** port for the **ESP32-C3**.

The ESP32-C3 base port was developed by our colleague **GabrielPCamargo** and its main reference is the repository:

- [GabrielPCamargo/nke_esp32c3](https://github.com/GabrielPCamargo/nke_esp32c3)

This README only exists to place the current branch in the context of the **Real-Time Systems** course repository from the **Computer Engineering program at UERGS**. Detailed explanations about the ESP32-C3 port, the original project organization, implementation decisions, and complete instructions should be consulted in Gabriel's repository README.

For more details about the implementation developed in this branch, see [`timer.md`](./timer.md).

## About This Branch

Unlike the previous branches in this repository, this version is **no longer based on NKE Arduino**.

The code is organized as a C implementation for the ESP32-C3, using the bare-metal build structure in `mdk/`. This branch starts from the base NKE for ESP32-C3 and adds work related to timer/counter support using the ESP32-C3 system timer.

In practical terms, this branch represents:

- an implementation on top of the base ESP32-C3 NKE port;
- use of the ESP32-C3 as the execution platform instead of Arduino/AVR;
- timer/counter-oriented changes for the NKE running on this target;
- continued use of Gabriel's project as the main reference for the ESP32-C3 port itself.

## Basic Structure

- `src/NKE_TIMER.c`: current timer/counter implementation built on top of the ESP32-C3 NKE base.
- `src/NKE.c`: base/reference NKE code for the ESP32-C3 version.
- `src/Makefile`: build configuration for the `esp32c3` target.
- `build.sh`: helper script to build from the repository root.
- `timer.md`: additional notes and explanations about the timer/counter implementation in this branch.
- `mdk/`: bare-metal structure used for ESP32/ESP32-C3 build and support.
- `include/`: auxiliary definitions used by the port, including ESP32-C3 register mappings.

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
