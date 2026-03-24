<p align="center">
<img src=".burstly_logo_full.png" width="330" style="display:block;margin:0 auto;">
</p>

**Burstly** is a high‑precision ESP‑IDF firmware project designed to control an electric heating element using **full‑wave AC modulation**.  
It exposes a minimal REST API for real‑time interaction and implements a deterministic **10 ms full‑wave switching schedule**, suitable for new generation smart‑meter with integration times below 30ms.

Burstly runs two dedicated tasks — a WiFi/API coordinator and a real‑time SSR controller — delivering robust, predictable AC control patterns based on Bresenham distribution and configurable control modes.

If no valid external control command is received within the configured watchdog timeout, the system automatically falls back to **safe OFF mode**, ensuring safety (with temperature protection assumed to be handled by an upper‑layer automation system).

This branch is optimized for the ISKRA AM550, which features a short integration time of 30ms. If you require a more precise or highly customizable solution, please use the main branch instead.


## Overview
Burstly is optimized for:

- half‑wave SSR control (default 10 ms)
- smart‑meter integration and PV power balancing
- multiple regulation modes (OFF, NETZERO, SOFTZERO, BURST)
- safe integration into home‑automation platforms (Home Assistant, etc.)
- deterministic real‑time behavior and watchdog supervision



## System Architecture

After initialization of core components (`status_led`, `wifi`, `ssr`), two primary tasks are launched:
 - wifi_coordinator
 - ssr_coordinator


### WiFi Coordinator (Core 0)
- Manages WiFi connectivity  
- Hosts the REST API  
- Validates request parameters  
- Sends control messages via queue to the SSR task  
- Drives status LED for connectivity/HTTP errors  

This task is **not time‑critical**.


### Real‑Time SSR Coordinator (Core 1)

A fully deterministic, timing‑critical task:
- Executes on a strict **HALF_WAVE** interval (10 ms default)  
- Uses fixed patterns for controlling the SSR  
- Processes new control messages during the 10ms delay before the new INTEGRATION_TIME interval starts  
- Repeats the current pattern if no new message arrives  
- Triggers **safety off** if no API command was received within `SSR_WATCHDOG_S`  
- If pattern computation ever exceeds timing (very unlikely), LED stays **solid yellow** until reboot  

This guarantees stable AC waveform timing independent of computation time.


## REST API

Burstly exposes two minimal endpoints.


### `GET /set` — Update Control State
Updates the SSR control parameters and triggers pattern generation.

#### Parameters

| Name       | Type  | Description |
|------------|--------|-------------|
| `mode`     | enum   | One of `MODE_OFF`, `MODE_BURST`, `MODE_NETZERO`, `MODE_SOFTZERO` |
| `p_active` | float  | Active power from smart meter (positive = export, negative = import) |


#### Response
- **200 OK** — Valid parameters; response includes calculated `ssr_lvl`
- **400 Bad Request** — Missing or invalid parameters

---

### `GET /reboot` — Restart Device
Reboots the ESP32.


## Control Modes

```c
typedef enum {
    MODE_INVALID,
    MODE_OFF,
    MODE_BURST,
    MODE_NETZERO,
    MODE_SOFTZERO
} ssr_mode_t;
```

### MODE_UNKNOWN  
Returned when invalid API parameters were provided.

### MODE_OFF  
SSR is always OFF.

### MODE_BURST  
High‑power heating (e.g., manual boost cycles).  

### MODE_NETZERO 
Attempts to use zero grid import.  
Uses:

### MODE_SOFTZERO  
Similar to NETZERO, but with a more conservative approach. It allows a grid import of up to `P_BOILER_LEVEL_MIN / 2` before throttling down. It follows the principle: 'Better to import 400W than to export 400W.' This mode is ideal for winter months or days with low PV yield.



## Integration Time (Why It Matters)

Smart meters do not measure instantaneous power directly; they calculate energy over a fixed **integration time frame** (e.g., 180 ms).

To avoid misleading measurements:
- ON‑waves must be evenly distributed  
- bursts must not be clustered  
- SSR output must match smart‑meter averaging models  

In an ideal world, the integration time is approximately 200ms, allowing for 10 control steps at full-wave resolution. However, modern smart meters like the Iskra AM550 integrate over just 30ms, which limits the resolution to 3 control steps at half-wave resolution.


## Watchdogs & Safety

### SSR Watchdog  
If no valid `/set` request is received within `SSR_WATCHDOG_S` seconds → fallback to **MODE_OFF**.

### (Environment) Thermal Protection  
Handled externally (e.g., via Home Assistant). Burstly only regulates energy flow!


## Status LED
- **Red** — indicates MODE_OFF
- **Green** — indicates MODE_NETZRO  
- **Blue** — indicates MODE_SOFTZERO 
- **Magenta** — indicates MODE_BURST
- **Yellow** — indicates a timing error
- **Cyan** — indicates init error
- **White** — indicates an imbalance in half-wave distribution, which could lead to a DC offset.
- **Off** — Controller inactive


## Control Message Structure

```c
typedef struct {
  ssr_mode_t mode;
  float p_active;
} ssr_control_msg_t;
```

Messages are transferred via queue from the API task to the SSR real‑time task.


## Kconfig Options

### SSR  
- `SSR_GPIO`  
- `SSR_WATCHDOG_S`

### Power
- `P_BOILER_MAX_NOM`  

### WiFi
- `WIFI_SSID`  
- `WIFI_PASSWORD`  
- `WIFI_MAX_STACK_RESTARTS`

## Hardware Requirements

Burstly is designed to run on a **dual‑core ESP32 MCU**.  
Because the project uses **task pinning** (WiFi/API on Core 0 and real‑time SSR control on Core 1),  
a **single‑core variant will not work**.

### Recommended MCU Module
- **ESP32‑S3‑WROOM‑1**  
  Provides dual‑core operation, stable WiFi performance, and sufficient timing accuracy for full‑wave SSR control.


### Recommended SSR
Burstly controls a **Zero‑Cross Solid State Relay (SSR)** to switch full AC waves.  
A proper zero‑cross SSR is required for clean waveform switching and predictable timing.

If the SSR requires **more than 3V input control voltage**, a **level shifter** must be used.  
Many SSRs accept 3–32V input and can be driven directly from an ESP32 GPIO.

Example compatible unit: **Carlo Gavazzi RGC1A23D30KKE**

Any equivalent SSR can be used as long as:
- it is **zero‑cross**
- it supports **3.0V input**, or a level shifter is added
- it is designed for **resistive loads**
- its output rating safely covers the **maximum heating power**, ideally ×1.5–2 to account for derating and thermal overhead


# Disclaimer

This software is provided **"as is"**, without any warranties of any kind, express or implied.  
Use of this firmware is entirely **at your own risk**.

This project is developed as a **personal side project**.  
There is **no support commitment**, **no guaranteed response time**, and **no obligation to implement feature requests or enhancements**.

If you decide to use or modify this software, you are fully responsible for testing, validating, and ensuring safe operation within your own environment.
