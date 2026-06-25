# STM32H7 Dual-Core USB Audio DSP – PEQ & FFT Spectrum Analyzer

**Dual-core STM32H7 USB sound card featuring a 10-band parametric equalizer (Cortex-M7) and a real-time FFT spectrum analyzer via Node-RED (Cortex-M4).**

This project presents a USB sound card prototype built around the STM32H755ZI-Q dual-core microcontroller. The system receives digital audio from a PC via USB Audio Class 1.0 (asynchronous mode at 48 kHz), processes it in real-time, and outputs it through an external DAC (PCM5102).

The architecture leverages the STM32H7 dual-core capabilities:
- **Cortex-M7**: Handles USB communication, 10-band parametric equalizer (IIR biquad filters), and audio output via SAI/I2S.
- **Cortex-M4**: Computes a 4096-point FFT, applies a Hann window to reduce spectral leakage, extracts 128 logarithmic frequency bands, and transmits the spectrum to a Node-RED dashboard via USART3.

The system is recognized natively as a USB audio device, plays audio through the PCM5102 DAC, applies real-time EQ profiles (Flat, Radio, Bass Boost), and displays a dynamic spectrum analyzer on a web dashboard – with no additional drivers required.

---

## Features

- **10‑Band Parametric Equalizer** – Cascaded IIR biquad filters with 3 selectable acoustic profiles (Flat, Radio, Bass Boost).
- **Real‑Time FFT Analysis** – 4096‑point FFT with Hann window, 128 logarithmic bands for human‑ear perception.
- **Asynchronous USB Feedback** – Implements UAC 1.0 feedback endpoint (10.14 format) for clock synchronization and jitter compensation.
- **Node‑RED Dashboard** – Visualizes 128 spectrum bars in real‑time via USART3 (115200 bps). Built with Vue.js and HTML/CSS.
- **Dual‑Core Architecture** – Cortex-M7 handles time‑critical audio tasks; Cortex-M4 handles heavy FFT math.
- **Memory Protection Unit (MPU)** – Ensures cache coherence between cores and DMA buffers.

---

## Hardware Requirements

| Component | Description |
|-----------|-------------|
| **MCU** | STM32H755ZI-Q (Nucleo‑H755ZI‑Q board) |
| **DAC** | PCM5102 (I2S interface) |
| **USB** | Native USB Full‑Speed (UAC 1.0) |
| **Display (optional)** | Node‑RED dashboard via USART3 |

---

## Software Stack

- **IDE**: STM32CubeIDE
- **Middleware**: STM32 USB Device Library (UAC 1.0)
- **DSP Library**: ARM CMSIS‑DSP (IIR biquad filters, FFT)
- **Visualization**: Node‑RED with Vue.js `ui-template`
- **Protocol**: USART3 (115200 bps, ASCII packets)

---

## How It Works

1. **USB Audio Reception**: The PC sends 48 kHz, 24‑bit stereo PCM packets via the isochronous OUT endpoint (0x01).
2. **Feedback Synchronization**: Every 16 ms, the Cortex‑M7 calculates the buffer gap and adjusts the feedback value (10.14 format) sent through the IN endpoint (0x81), keeping the audio buffer stable.
3. **DSP Processing**: Each sample is processed through a 10‑stage IIR biquad cascade (PEQ). Signal is saturated at ±0.99 and then attenuated by 20% (headroom).
4. **DAC Output**: Processed samples are scaled to 32‑bit integers (`2³¹‑1`) and written to a circular buffer (`SaiPlaybackBuffer`). DMA streams the data to the SAI/I2S peripheral and PCM5102 DAC.
5. **FFT & Visualization**: The M4 core reads mono samples from shared SRAM4, applies a Hann window, computes the FFT, maps bins to 128 logarithmic bands, and sends the spectrum to Node‑RED every 50 ms.

---

## License

This project is licensed under the **MIT License** – feel free to use, modify, and distribute it.

> *The STM32 HAL and middleware files retain their original STMicroelectronics copyright notices (BSD‑3‑Clause and SLA0044).*

---

## References

1. STMicroelectronics, *RM0399 – STM32H7 Reference Manual*, 2021.
2. STMicroelectronics, *AN5557 – STM32H7 Dual‑Core Architecture*, 2020.
3. STMicroelectronics, *UM2217 – STM32H7 HAL and Low‑Layer Drivers*, 2022.
4. STMicroelectronics, *UM2408 – STM32H7 Nucleo‑144 Boards User Manual*, 2025.
5. STMicroelectronics, *STM32H755xI Datasheet*, 2026.
6. USB Implementers Forum, *USB Specification Revision 1.1*, 1998.
7. USB Implementers Forum, *USB Specification Revision 2.0*, 2000.
8. USB Implementers Forum, *USB Audio Device Class Definition 1.0*, 1998.
9. USB Implementers Forum, *USB Audio Device Class Definition 2.0*, 2006.
10. USB Implementers Forum, *USB Audio Data Formats 1.0*, 1998.
11. ARM Limited, *CMSIS‑DSP Software Library*. Available: https://arm-software.github.io/CMSIS-DSP/main/
12. ARM Limited, *CMSIS‑DSP – Biquad Cascade IIR Filters*. Available: https://arm-software.github.io/CMSIS-DSP/main/group__BiquadCascadeDF1.html
13. ARM Limited, *CMSIS‑DSP – Real FFT Functions*. Available: https://arm-software.github.io/CMSIS-DSP/main/group__RealFFT.html
14. R. Bristow-Johnson, *"Cookbook formulae for audio EQ biquad filter coefficients"*. Available: https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
15. National Instruments, *"Understanding FFTs and Windowings"*. Available: https://www.ni.com/en/shop/data-acquisition/measurement-fundamentals/analog-fundamentals/understanding-ffts-and-windowing.html
16. Node‑RED, *Node‑RED Documentation*. Available: https://nodered.org/docs/
17. Texas Instruments, *PCM5102 Datasheet*, 2012.
---

**Developed as a final-year engineering project at the University of Minho, Portugal.**
