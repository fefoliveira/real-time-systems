# Real-Time Systems

This repository gathers the code developed in the **Real-Time Systems** course, offered by the **Computer Engineering degree at UERGS**.

The proposal is to use **NKE (Nano Kernel Educational)** as a base and record, incrementally, the changes made to bring this educational nanokernel closer to real-time systems concepts and techniques.

The purpose of this README is not to re-explain the base NKE. The original architecture, educational motivation, and internal details of the NKE should be consulted in the original articles, videos, and repositories listed in the references. Here, the focus is to document the adaptations made during the course.

## Implementation

The implementation is being developed as an Arduino sketch in the file [`nke-base.ino`](./nke-base.ino).

The code starts from an NKE version adapted for Arduino/AVR and is expected to evolve throughout the course as new real-time techniques are studied and implemented.

Each part of the real-time implementation is organized in a separate branch. In this repository, branches work as a record of each step in the implementation of the real-time concepts studied throughout the course, making it easier to compare approaches and keep the history of each technique isolated.

For testing, the following tools can be used:

- **Arduino IDE**: https://www.arduino.cc/en/software
- **Wokwi**: https://wokwi.com/

Wokwi allows Arduino projects to be simulated directly in the browser, which is useful for testing and observing kernel behavior without always relying on the physical board.

## Branches

The branches are used as an implementation log for the course. Each one preserves a specific stage of the project, usually focused on one real-time systems concept or experiment.

- `master`: base NKE code used as the starting point for the real-time systems adaptations.
- `rate-monotonic`: first implementation developed in the project, focused on Rate Monotonic scheduling.
- `deadline-monotonic`: Deadline Monotonic implementation, created mainly as a study branch for the first exam.
- `aperiodic-polling-server`: polling server implementation for aperiodic tasks, built on top of the `rate-monotonic` branch.
- `polling-server-with-prio`: current experimental branch, based on the polling server approach and including an initial attempt at priority inheritance. This branch was only lightly explored, so it may be incomplete or not fully functional.
- `feature/esp32c3-base-nke`: base port of the NKE to the ESP32-C3 platform. This branch is no longer based on the Arduino/AVR version and uses a C bare-metal structure for ESP32-C3. The port was developed by GabrielPCamargo and is based on [GabrielPCamargo/nke_esp32c3](https://github.com/GabrielPCamargo/nke_esp32c3).
- `feature/esp32c3-nke-timer-counter`: implementation built on top of the base ESP32-C3 NKE port, focused on timer/counter support using the ESP32-C3 system timer. This branch includes additional notes about the implementation in `timer.md`.

## NKE References

- Costa, Celso Maciel da; Fragoso, João Leonardo; Matias Jr., Lucas Rivalino; Silva, Leonardo da Luz; Fracalossi, Aline; Brasil, Cássio; Debom, Guilherme. **NKE - Um Nanokernel Educacional para Microprocessadores ARM**. Available at: https://www.academia.edu/13003555/NKE_Um_Nanokernel_Educacional_para_Microprocessadores_ARM
- Costa, Celso Maciel da; Fragoso, João Leonardo; Matias Jr., Lucas Rivalino; Silva, Leonardo da Luz; Fracalossi, Aline; Brasil, Cássio; Debom, Guilherme. **NKE - Um Nanokernel Educacional para Microprocessadores ARM**. SBESC 2014 Proceedings. Available at: https://sbesc.lisha.ufsc.br/sbesc2014/dl225
- jeisonmp. **NKE0.8a - Nanokernel para ARM-LPC2378, version 0.8a**. Available at: https://github.com/jeisonmp/NKE0.8a
- Portal de Periódicos da Univali. **Additional reference related to the NKE materials indicated**. Available at: https://periodicos.univali.br/index.php/acotb/article/view/21088/12159
- **Video related to NKE**. Available at: https://www.youtube.com/watch?v=pD51msNip78

## Supporting References

- Oliveira, Rômulo Silva de. **Real-Time Systems**.
- Liu, C. L.; Layland, J. W. **Scheduling Algorithms for Multiprogramming in a Hard-Real-Time Environment**.

## Institution

**Universidade Estadual do Rio Grande do Sul (UERGS)**  
Course: **Computer Engineering**  
Discipline: **Real-Time Systems**
