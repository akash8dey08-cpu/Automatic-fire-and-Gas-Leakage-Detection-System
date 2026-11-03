[readme.md](https://github.com/user-attachments/files/34177584/readme.md)
# Multi-Sensor Adaptive Electronic Nose for Autonomous Fire & Gas Hazard Mitigation with Real-Time ML Telemetry
[[https://img.shields.io/badge/%F0%9F%93%84%20Read%20Technical%20Paper-IEEE%20Format-004080?style=for-the-badge&logo=adobeacrobatreader&logoColor=white]](Adaptive_MultiSensor_Electronic_Nose.pdf)
[[https://img.shields.io/badge/%F0%9F%93%98%20Project%20Report-Doc-1B365D?style=for-the-badge]](Adaptive_E_nose_Project_Report.docx)
[[https://img.shields.io/badge/Firmware-Arduino%20C%2B%2B-00979D?style=for-the-badge&logo=arduino&logoColor=white]](Arduino_code.ino)

> **📄 [Click Here to View the Full Technical Paper](Adaptive_MultiSensor_Electronic_Nose.pdf)**
> *An embedded safety and data-engineering system that bridges real-time signal conditioning, multi-sensor pattern fusion, autonomous electromechanical hazard mitigation, and 1 Hz 17-feature CSV data logging for supervised Machine Learning models.*

---

## Executive Overview

Conventional microcontroller gas detectors rely on single-sensor raw analog thresholds that fail under **ADC noise**, **cross-sensitivity across hydrocarbon gases**, and **long-term baseline drift** driven by ambient temperature and humidity shifts. Furthermore, traditional embedded alarm circuits operate as closed loops that discard intermediate signal states rather than generating structured datasets for Machine Learning (ML).

This project implements a **Multi-Sensor Adaptive Electronic Nose** on the **Arduino Uno R3 (`ATmega328P`)** that simultaneously performs **autonomous physical hazard mitigation** and streams **pre-conditioned 17-feature CSV telemetry at 1 Hz (`9600 baud`)** for downstream ML training and inference.

### Key Engineering Highlights

* **Dual-Stage Exponential Moving Average (EMA) Signal Conditioning:**
  * **Stage 1 — Fast Signal EMA ($\alpha_{\text{signal}} = 0.20$):** Attenuates single-sample ADC and thermal spikes by **80%** on arrival while preserving rapid step-response ($>89\%$ steady-state amplitude within 10 cycles).
  * **Stage 2 — Conditional Slow Baseline EMA ($\alpha = 0.01$):** Continuously tracks slow ambient drift during verified safe, quiescent periods ($\max_k |S_{\text{norm},k}| < 0.05$) and **freezes immediately** during gas excursions so slow leaks are never absorbed into the baseline.
* **4-Class Sensor Fusion + 2000 ms Temporal Confirmation (Debounce):**
  * Normalizes filtered signals relative to dynamic baselines across **MQ2**, **MQ5**, and **MQ7** to classify **Carbon Monoxide (`CO`)**, **LPG / Propane**, **Smoke / Fire**, and **Methane / Natural Gas (`NG`)**.
  * Enforces a **2000 ms continuous persistence filter** before promoting a candidate signature to a confirmed hazard, eliminating false alarms from brief airflow transients.
* **Hardware-Protected Autonomous Actuation & Anti-Flicker UI:**
  * Drives a 5V DC exhaust fan via a low-side **2N2222 NPN transistor switching stage** ($1\text{ k}\Omega$ base resistor) protected against inductive back-EMF kickback by a **1N4007 flyback diode**, alongside an active buzzer and a state-buffered anti-flicker **16x2 I2C LCD (`0x27`)**.
* **Production 17-Feature ML CSV Telemetry Pipeline:**
  * Streams synchronized timestamps, raw ADC counts, EMA-filtered signals, dynamic baselines, normalized relative deviations, ambient temperature ($^\circ\text{C}$), relative humidity ($\%\text{RH}$), binary optical flame status, and confirmed ground-truth labels at **1 Hz**.

---

## System Architecture



### Four-Layer Real-Time Execution Pipeline (`1 Hz`)

1. **Multi-Modal Sensing Layer:** Samples 10-bit ADC channels (`MQ2` on `A0`, `MQ5` on `A1`, `MQ7` on `A2`), active-low IR flame status (`D4`), digital 1-Wire temperature and relative humidity (`DHT22` on `D3` with $10\text{ k}\Omega$ pull-up), and user tactile input (`D7` with `INPUT_PULLUP`).
2. **Dual-Stage EMA Conditioning & Normalization Layer:** Performs a 50-sample clean-air auto-calibration at boot ($\Delta t = 100\text{ ms}$), filters raw analog signals ($\alpha_{\text{signal}} = 0.20$), computes dimensionless relative responses $S_{\text{norm}}$, and conditionally adapts clean-air baselines ($\alpha = 0.01$).
3. **Pattern Fusion & Temporal Debounce Layer:** Evaluates cross-sensitivity rules across $\mathbf{X}(t) = [\text{MQ2}_{\text{norm}}, \text{MQ5}_{\text{norm}}, \text{MQ7}_{\text{norm}}]^T$ and validates candidate persistence over $T_{\text{confirm}} = 2000\text{ ms}$.
4. **Actuation & ML Serialization Layer:** Triggers the buzzer (`D5`), fan driver (`D6`), and priority-ordered LCD messages (`A4/A5`), and transmits a 17-column CSV frame over UART (`9600 baud`).

---

## Mathematical Formulation

| Stage | Mathematical Expression | Engineering Purpose |
| :--- | :--- | :--- |
| **1. Startup Auto-Calibration** | $S_{\text{base},k}(0) = \frac{1}{50}\sum_{i=1}^{50} S_{\text{raw},k}(i), \quad S_{\text{filt},k}(0) = S_{\text{base},k}(0)$ | Establishes clean-air reference over $5.0\text{ s}$ and eliminates startup filter lag. |
| **2. Fast Signal EMA** | $S_{\text{filt},k}(t) = 0.20 \cdot S_{\text{raw},k}(t) + 0.80 \cdot S_{\text{filt},k}(t-1)$ | Suppresses high-frequency ADC noise prior to normalization. |
| **3. Relative Normalization** | $S_{\text{norm},k}(t) = \frac{S_{\text{filt},k}(t) - S_{\text{base},k}(t-1)}{S_{\text{base},k}(t-1)}$ | Converts raw ADC counts into dimensionless fractional deviation. |
| **4. Conditional Baseline EMA** | $S_{\text{base},k}(t) = 0.99 \cdot S_{\text{base},k}(t-1) + 0.01 \cdot S_{\text{raw},k}(t)$ iff $\max_j |S_{\text{norm},j}| < 0.05$ | Tracks slow thermal/aging drift in clean air; freezes during leaks. |
| **5. Temporal Debounce** | $C_{\text{det}}(t) = C_{\text{cand}}(t)$ iff $C_{\text{cand}}(t) \neq \text{None} \land (t - t_{\text{start}}) \ge 2000\text{ ms}$ | Rejects transient spikes shorter than 2 seconds. |



---

## Hardware Pinout & Protection Circuitry



| Component | Component Pin | Arduino Uno Pin | Electrical Configuration & Protection Notes |
| :--- | :--- | :--- | :--- |
| **MQ2 Gas Sensor** | `AO`, `VCC`, `GND` | `A0` (Analog) | 10-bit ADC ($0\text{--}5\text{ V}$); senses LPG, Propane, $\text{H}_2$, Smoke |
| **MQ5 Gas Sensor** | `AO`, `VCC`, `GND` | `A1` (Analog) | 10-bit ADC ($0\text{--}5\text{ V}$); senses Methane, Natural Gas, LPG |
| **MQ7 Gas Sensor** | `AO`, `VCC`, `GND` | `A2` (Analog) | 10-bit ADC ($0\text{--}5\text{ V}$); senses Carbon Monoxide (`CO`) |
| **DHT22 Sensor** | `DATA`, `VCC`, `GND` | `D3` (Digital) | External **$10\text{ k}\Omega$ pull-up resistor** between `DATA` and $+5\text{ V}$ |
| **IR Flame Sensor** | `DO`, `VCC`, `GND` | `D4` (Digital) | Active-LOW digital comparator output (`LOW` = Fire Detected) |
| **Buzzer Module** | `IN`, `VCC`, `GND` | `D5` (Digital) | Active-HIGH piezoelectric alarm trigger |
| **Fan Driver Stage** | `2N2222 Base` | `D6` (Digital) | `D6` $\rightarrow$ **$1\text{ k}\Omega$ resistor** $\rightarrow$ `2N2222 Base`; **1N4007 flyback diode** across fan |
| **Push Button** | `Terminal 1 / 2` | `D7` (Digital) | Configured with internal pull-up (`INPUT_PULLUP`; other side to `GND`) |
| **16x2 I2C LCD** | `SDA`, `SCL` | `A4` (`SDA`), `A5` (`SCL`) | Hardware I2C (`0x27`); state-buffered `updateLCD()` prevents screen flicker |

---

## Machine Learning CSV Telemetry Schema (`1 Hz`, `9600 Baud`)

Upon completing initialization, the microcontroller outputs the 17-column header followed by one comma-delimited feature vector every `1000 ms`:

```csv
time,raw1,raw2,raw3,filt1,filt2,filt3,base1,base2,base3,norm1,norm2,norm3,temp,humidity,fire,gas
6012,218,194,165,218.2,194.1,165.0,218.0,194.0,165.0,0.0009,0.0005,0.0000,25.6,58.2,0,None
7014,217,195,164,218.0,194.3,164.8,218.0,194.0,165.0,-0.0002,0.0014,-0.0012,25.6,58.2,0,None
10019,230,214,276,224.2,202.5,206.4,218.0,194.0,165.0,0.0284,0.0438,0.2509,25.7,58.0,0,None
12023,232,216,288,226.8,207.2,235.1,218.0,194.0,165.0,0.0404,0.0680,0.4248,25.7,57.9,0,Carbon Monoxide
16031,364,354,165,298.9,284.1,195.0,218.0,194.0,165.0,0.3711,0.4644,0.1818,25.9,57.2,0,LPG/Propane
17033,368,359,165,312.7,299.1,189.0,218.0,194.0,165.0,0.4344,0.5418,0.1455,26.4,56.8,1,LPG/Propane
```

| Columns | Feature Names | Data Type | Role in Downstream ML Pipeline |
| :--- | :--- | :--- | :--- |
| **Col 1** | `time` | `uint32` (ms) | Monotonic boot timestamp for time-series windowing and slope ($\Delta S / \Delta t$) extraction |
| **Cols 2–4** | `raw1, raw2, raw3` | `int` (0–1023) | Unfiltered 10-bit ADC readings from `MQ2`, `MQ5`, and `MQ7` |
| **Cols 5–7** | `filt1, filt2, filt3` | `float` (1 dec) | Fast EMA-smoothed sensor trajectories ($\alpha_{\text{signal}} = 0.20$) |
| **Cols 8–10** | `base1, base2, base3` | `float` (1 dec) | Adaptive clean-air baseline references ($\alpha = 0.01$) |
| **Cols 11–13** | `norm1, norm2, norm3` | `float` (4 dec) | Dimensionless normalized relative responses; **primary input features for classification** |
| **Cols 14–15** | `temp, humidity` | `float` (1 dec) | Ambient temperature ($^\circ\text{C}$) and relative humidity ($\%\text{RH}$) for thermodynamic compensation |
| **Col 16** | `fire` | `binary` (`0`/`1`) | Optical IR flame indicator (`1` = active flame detected on `D4`) |
| **Col 17** | `gas` | `string` | Temporally confirmed class label (`None`, `Carbon Monoxide`, `LPG/Propane`, `Smoke/Fire`, `Methane/NG`) |

---

## Multi-Sensor Operating Regime Matrix

| Operating Regime | `MQ2_norm` | `MQ5_norm` | `MQ7_norm` | Flame (`D4`) | Confirmed Label (`gas`) | Autonomous Mitigation & LCD Status |
| :--- | :---: | :---: | :---: | :---: | :--- | :--- |
| **Clean Air (Quiescent)** | $< 0.05$ | $< 0.05$ | $< 0.05$ | `HIGH` (`0`) | `None` | Buzzer `OFF`, Fan `OFF`, Baseline EMA Adapting; LCD: `System Safe / Temp: 25.60C` |
| **Carbon Monoxide (CO)** | $< 0.10$ | Low / Mod | $> 0.20$ | `HIGH` (`0`) | `Carbon Monoxide` | Buzzer `ON`, Fan `ON`, Baseline Frozen; LCD: `DANGER! / Gas: Carbon Mono` |
| **LPG / Propane Leak** | $> 0.20$ | $> 0.20$ | $< 0.05$ | `HIGH` (`0`) | `LPG/Propane` | Buzzer `ON`, Fan `ON`, Baseline Frozen; LCD: `DANGER! / Gas: LPG / Propa` |
| **Smoke / Incipient Fire** | $> 0.20$ | $< 0.10$ | $0.05\text{--}0.18$ | `HIGH` (`0`) | `Smoke/Fire` | Buzzer `ON`, Fan `ON`, Baseline Frozen; LCD: `DANGER! / Gas: Smoke / Fir` |
| **Methane / Natural Gas** | $< 0.15$ | $> 0.20$ | $< 0.05$ | `HIGH` (`0`) | `Methane/NG` | Buzzer `ON`, Fan `ON`, Baseline Frozen; LCD: `DANGER! / Gas: Methane/NG` |
| **Direct Flame (+ Gas)** | Variable | Variable | Variable | `LOW` (`1`) | `<detectedGas>` | Buzzer `ON`, Fan `ON`, Baseline Frozen; LCD: `DANGER! / FIRE & <Gas>` or `Fire Detected!` |

---

## Repository Structure & Documentation

* 📄 **[Technical Paper (PDF)](Adaptive_MultiSensor_Electronic_Nose.pdf)** — IEEE-style research paper.
* 📘 **[Engineering Project Report (DOCX)](Adaptive_E_nose_Project_Report.docx)** — Full project report.
* ⚙️ **[Arduino Firmware](Arduino_code.ino)** — Production embedded C++ code.
