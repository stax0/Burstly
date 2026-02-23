# Burstly – High‑Precision Full‑Wave Control for Smart Heating

**Burstly** is a high‑precision ESP‑IDF firmware project designed to control an electric heating element using **full‑wave AC modulation**.  
It exposes a minimal REST API for real‑time interaction and implements a deterministic **20 ms full‑wave switching schedule**, suitable for smart‑meter–driven energy management and PV self‑consumption optimization.

Burstly runs two dedicated tasks — a WiFi/API coordinator and a real‑time SSR controller — delivering robust, predictable AC control patterns based on Bresenham distribution and configurable control modes.

If no valid external control command is received within the configured watchdog timeout, the system automatically falls back to **safe OFF mode**, ensuring safety (with temperature protection assumed to be handled by an upper‑layer automation system).


## Overview
Burstly is optimized for:

- full‑wave SSR control (default 20 ms)
- smart‑meter integration and PV power balancing
- multiple regulation modes (Burst, Downshift, Upshift, Sigma‑Delta)
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
- Executes on a strict **FULL_WAVE** interval (20 ms default)  
- Uses Bresenham algorithm to distribute ON‑waves uniformly  
- Processes new control messages during the 20ms delay before the new INTEGRATION_TIME interval starts  
- Repeats the current pattern if no new message arrives  
- Triggers **safety off** if no API command was received within `SSR_WATCHDOG_S`  
- If pattern computation ever exceeds timing (very unlikely), LED stays **solid orange** until reboot  

This guarantees stable AC waveform timing independent of computation time.


## REST API

Burstly exposes two minimal endpoints.


### `GET /set` — Update Control State
Updates the SSR control parameters and triggers pattern generation.

#### Parameters

| Name       | Type  | Description |
|------------|--------|-------------|
| `mode`     | enum   | One of `MODE_OFF`, `MODE_BURST`, `MODE_DOWNSHIFT`, `MODE_UPSHIFT`, `MODE_SIGMADELTA` |
| `p_active` | float  | Active power from smart meter (positive = import, negative = export) |
| `p_boiler` | float  | Current heating power; if p_boiler < 1 → uses `P_BOILER_MAX_NOM` |

#### Response
- **200 OK** — Valid parameters; response includes calculated `ssr_lvl`
- **400 Bad Request** — Missing or invalid parameters

---

### `GET /reboot` — Restart Device
Reboots the ESP32.


## Control Modes

```c
typedef enum {
    MODE_UNKNOWN = 0,
    MODE_OFF,
    MODE_BURST,
    MODE_DOWNSHIFT,
    MODE_UPSHIFT,
    MODE_SIGMADELTA
} ssr_mode_t;
```

### MODE_UNKNOWN  
Returned when invalid API parameters were provided.

### MODE_OFF  
SSR is always OFF.

### MODE_BURST  
High‑power heating (e.g., manual boost cycles).  
Uses `SSR_LVL_BURST` as configured via Kconfig.

### MODE_DOWNSHIFT  
Attempts to reduce grid import to ~0 W.  
Uses:
```c
(uint8_t)ceilf((float)SSR_STEPS * ratio)
```

### MODE_UPSHIFT  
Maximizes self‑consumption even if grid import (positive p_active) is required.  
Uses:
```c
(uint8_t)floorf((float)SSR_STEPS * ratio)
```

### MODE_SIGMADELTA  
Balanced mode minimizing both import and export, inspired by sigma‑delta modulation.


## Integration Time (Why It Matters)

Smart meters do not measure instantaneous power directly; they calculate energy over a fixed **integration time frame** (e.g., 180 ms).

To avoid misleading measurements:
- ON‑waves must be evenly distributed  
- bursts must not be clustered  
- SSR output must match smart‑meter averaging models  


Therefore:
- `INTEGRATION_TIME` controls the pattern calculation window  
- Bresenham ensures even full‑wave spacing  


## Watchdogs & Safety

### SSR Watchdog  
If no valid `/set` request is received within `SSR_WATCHDOG_S` seconds → fallback to **MODE_OFF**.

### (Environment) Thermal Protection  
Handled externally (e.g., via Home Assistant). Burstly only regulates energy flow!


## Status LED

- **Yellow solid** — WiFi or HTTP subsystem error, trying to recover
- **Red solid** — WiFi or HTTP subsystem error, ESP will reboot
- **Orange solid** — Pattern calculation overrun / critical system error / calculation algorithm needs to be adjusted 
- **Green LED on/flashing** — SSR output currently energized  
- **Off** — SSR inactive


## Control Message Structure

```c
typedef struct {
  ssr_mode_t mode;
  float p_active;
  float p_boiler;
} ssr_control_msg_t;
```

Messages are transferred via queue from the API task to the SSR real‑time task.


## Kconfig Options

### SSR
- `SSR_LVL_MIN`
- `SSR_LVL_MAX`  
- `SSR_LVL_BURST`  
- `SSR_GPIO`  
- `SSR_DEBOUNCE_ON`
- `SSR_DEBOUNCE_OFF`  
- `SSR_WATCHDOG_S`

### Power
- `P_BOILER_MAX_NOM`  
- `P_HYSTERESE_BYPASS`  
- `INTEGRATION_TIME`  
- `FULL_WAVE`

### WiFi

- `WIFI_SSID`  
- `WIFI_PASSWORD`  
- `WIFI_MAX_STACK_RESTARTS`



# Disclaimer

This software is provided **"as is"**, without any warranties of any kind, express or implied.  
Use of this firmware is entirely **at your own risk**.

This project is developed as a **personal side project**.  
There is **no support commitment**, **no guaranteed response time**, and **no obligation to implement feature requests or enhancements**.

If you decide to use or modify this software, you are fully responsible for testing, validating, and ensuring safe operation within your own environment.