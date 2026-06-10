[![License: MT-SAL v1.0](https://img.shields.io/badge/License-MT--SAL%20v1.0-blue)](https://github.com/rafamuratt/Industrial_HMI_STM32_BluePill_F103C8T6_LCD16x2?tab=License-1-ov-file)


# Industrial_HMI_STM32_BluePill_F103C8T6_LCD16x2  
Developed entirely in Arduino IDE (STM32duino core) using C++, with one external LCD library (see note below). 
> The project is fully optimized for the STM32F103C8T6 (BluePill), keeping a lean and maintainable structure with room for future expansion. This is a functional industrial-style HMI example combining PWM motor control, pseudo analog output, analog input reading, EEPROM parameter saving, and emergency stop logic — designed as a reusable template for future embedded projects.  
✅ EMERGENCY STOP CONTROL IS IMPLEMENTED

---

## 🚀 System Overview

This project implements a generic industrial HMI control interface, featuring:

* **High-Speed PWM Output (PA8):** 20kHz switching frequency for silent motor speed control, inaudible to the human ear and suitable for most industrial actuators.
* **Pseudo Analog Output (PA9):** PWM-based average voltage output (0–3.3V), usable as a DAC substitute with an external RC low-pass filter (R = 4k7 to 10k, C = 1 to 10µF).
* **Analog Input Reading (PA1):** 10-bit ADC channel with real-time voltage display on the HMI (0–3.3V range), mapped and formatted as a fake floating point for the 16x2 LCD.
* **Dual-Mode Button UI (NAV_ADJ):** Short press for step-by-step parameter adjustment; 2-second long press flips adjustment direction (+ / −) and enables auto-scroll mode.
* **Manual EEPROM Save (SEL_ESC):** Parameters are saved to Flash only when the operator explicitly confirms inside the settings sub-menu, preventing accidental writes.
* **Atomic EEPROM Saving:** Interrupt-safe Flash emulation using noInterrupts() to suspend TIM2 and all other interrupts before writing, preventing CPU desync or mid-write corruption. Interrupts are restored immediately after. This also prevents unresponsive buttons during Flash erase cycles, which can stall the CPU for several milliseconds on STM32.
* **Emergency Stop System (PC14):** Hardware-driven emergency stop with immediate PWM flush (both outputs forced to 0V simultaneously via EGR register), timer pause, onboard LED (PC13) blink alert at 500ms, and <ACK> confirmation requirement (ACK) before the system can resume.
* **16x2 LCD HMI Interface:** Custom 4-bit Hitachi-compatible library on PortB with a scannable menu tree including Run/Stop control, motor speed adjustment, analog input monitoring, and system info.

---

## ⚠️ External Library Required

The LCD interface uses a custom 4-bit library (MT_lcd_16x2_4bits_STM32.h) developed separately and available here:<br>
👉 https://github.com/rafamuratt/HD44780_4bit_STM32  
Download and place it in your Arduino libraries folder before compiling.

---

## 🛠 Hardware Stack

* **MCU:** STM32F103C8T6 (BluePill)
* **Display:** Hitachi-compatible 16x2 LCD (4-bit mode, PortB)
* **Development Environment:** Arduino IDE with STM32duino core
* **Language:** C++ (Arduino framework)
* **PWM Output:** PA8 — 20kHz, TIM1 Channel 1
* **Pseudo Analog Output:** PA9 — TIM1 Channel 2 + external RC filter
* **Analog Input:** PA1 — 10-bit ADC
* **Emergency Stop Input:** PC14 — active LOW, monitored via TIM2 ISR
* **Onboard LED: PC13** — reverse logic (HIGH = OFF)

Button Mapping
|  Define | Pin |                      Function                       |
|---------|-----|-----------------------------------------------------|
| SEL_ESC | PA0 | Confirm, enter sub-menu, save settings, ACK, escape |
| NAV_ADJ | PA2 | Scroll menu, adjust value, toggle Run/Stop          |

---

## 📂 Project Structure
```
/Source           — Main .ino source file with full comments
/Screenshots      — Screenshots and hardware photos
```

---

## ⚙ Operational Flow

* **Home Screen:** Displays system run state and a live incrementing counter (Process Variable simulation, updated every 250ms by TIM2 ISR).
* **Menu Navigation:** Press SEL_ESC to enter the menu list. Use NAV_ADJ to scroll through options. Press SEL_ESC again to enter a sub-menu.
  
* **Run / Stop      (Menu 1):* Short press NAV_ADJ to toggle the system between Running and Stopped states. On stop, PWM outputs are immediately flushed to 0V.
* **Return          (Menu 2):* Short press SEL_ESC to return to Home Screen.
* **Motor Speed     (Menu 3):* Short press NAV_ADJ steps the parameter by 3 units (≈1.2%). Long press (2s) flips direction (+ / −) and enables auto-scroll at 100ms intervals. Parameter is safely clamped to [0, 255] — no wrap-around, preventing dangerous power jumps on real loads. SEL_ESC saves the current parameters to Flash (pseudo EEPROM) and escape to the menu list.
* **Voltage Monitor (Menu 4):* Reads PA1 ADC in real time and displays the mapped voltage level (0.00–3.26V).
* **About           (Menu 5):* About page
  
**Emergency Stop:** Triggered by PC14 going LOW. Both PWM outputs are flushed to 0V simultaneously, the timer is paused, and the LCD locks on EMERGENCY STOP / <ACK>. The onboard LED blinks at 500ms. System resumes only after the physical EMS is released AND the operator confirms with SEL_ESC (ACK).  

**Parameter Mapping:** PWM duty = map(param1, 0, 255, 0, timerOverflow) — 0 to 100% output range.

---

## 📜 License

This project is licensed under the Murat-Tech Source Available License v1.0.
Free for personal and educational use with attribution.
Commercial use requires written authorization — contact info@murat-tech.eu

☕ If this project is helpful for your application, please consider supporting:<br>
https://www.paypal.com/donate/?hosted_button_id=8S8BJ9TT368VN  

Built by rafamuratt: https://murat-tech.eu/  
Murat-Tech Channel:  https://www.youtube.com/@Murat-TechChannel-EN
