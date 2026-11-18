<p align="center">
<img src=".burstly_logo_full.png" width="330">
</p>

Burstly is ESP32 firmware that feeds surplus PV power into an electric water heater. It switches a zero cross solid state relay (SSR) in single AC half-waves, so a smart meter with a short integration time sees as little grid import and export as possible.

In use since March 2026 on a 150 l boiler with a 2.35 kW heater.

## Why half-waves

* Most PV diverters switch the heater in bursts of a second or longer.
* Meters like the Iskra AM550 net import and export over 30 ms only. A full power burst is billed as import and the pause after it as export, even if the average is zero.
* 30 ms at 50 Hz are three half-waves. Burstly switches whole half-waves in a pattern that repeats every 30 ms, so every meter window sees the same power.
* This gives four levels: 0, 33, 66 and 100 %.

## Results

Estimates for the setup above. Other setups will differ.

| | |
|---|---|
| Savings | about 350 € per year |
| Hardware cost | about 80 € (SSR, ESP32, wiring) |
| Payback | about 3 months |

## How it works

```
+-----------------+  GET /set  +-------------+  queue  +-----------+
| Home automation | ---------> | HTTP server | ------> | SSR loop  |
|                 |            | core 0      |         | core 1    |
+-----------------+            +-------------+         +-----------+
         ^                                                   |
         | grid power                                        | GPIO
         |                                                   v
+-----------------+          +--------+          +----------------+
|   Smart meter   | <------- | Heater | <------- | Zero cross SSR |
+-----------------+          +--------+          +----------------+
```

* An external controller, for example Home Assistant, reads the grid power and calls `GET /set` every few seconds with the power and the mode.
* Core 0 runs WiFi and the HTTP server.
* Core 1 runs only the SSR loop. It switches the GPIO every 10 ms and takes new messages between two patterns.
* Without new messages the current pattern keeps running until the watchdog switches off.

### Level selection

```
p_eff = p_active + current_level * P_nominal
```

* `p_active` is the grid power, positive for export. Adding the current heater power gives the real surplus.
* `p_eff` is compared with the thresholds of the mode, see [Modes](#modes).
* Up: one level at a time, after `SSR_UPSHIFT_DEBOUNCE` messages in a row above the threshold.
* Down: at once.

### Half-wave patterns

```
slot        1  2  3  4  5  6
polarity    +  -  +  -  +  -
33 %        #  .  .  #  .  .
66 %        #  #  .  #  #  .
100 %       #  #  #  #  #  #
```

* One pattern has 6 slots of 10 ms, one half-wave per slot.
* Each pattern repeats after 3 slots, so every 30 ms meter window gets the same energy.
* DC free by design. At 33 % the two active half-waves have opposite polarity. At 66 % they come in pairs.
* The firmware counts active slots on even and odd positions and blocks the output if the counts differ.
* No mains sync needed. The zero cross SSR switches at the next zero crossing, so a slot that starts a bit early or late still covers exactly one half-wave.

## Modes

Thresholds for a 2350 W heater. The value is the surplus `p_eff` needed for each level.

| Mode | 33 % | 66 % | 100 % | Use |
|---|---|---|---|---|
| `MODE_OFF` | | | | Heater off |
| `MODE_BURST` | | | always | Full power, for example a boost or a legionella cycle |
| `MODE_NETZERO` | 775 W | 1551 W | 2350 W | Only real surplus, no grid import |
| `MODE_NETZERO_CAP` | 775 W | 1551 W | never | Like NETZERO but stops at 66 %. Spreads the heating over the day to catch peaks above the export limit |
| `MODE_SOFTZERO` | 129 W | 912 W | 1695 W | For days with little sun. Takes every watt of PV and accepts up to about 650 W from the grid |

SOFTZERO goes to the next level once the surplus is 1/6 of a level above the current one. On a bad day the boiler needs grid power anyway. It is cheaper to buy a bit now than to export the small surplus for little money and buy it back in the evening.

## REST API

### `GET /set`

| Parameter | Description |
|---|---|
| `mode` | `mode_off`, `mode_burst`, `mode_netzero`, `mode_netzero_cap` or `mode_softzero` |
| `p_active` | Grid power in W. Positive means export. Not needed for `mode_off` |

```
$ curl "http://<esp-ip>/set?mode=mode_softzero&p_active=350"
{"mode":"mode_softzero","p_active":350.00,"ssr_lvl":33.30}
```

The answer contains the level the firmware picked. Bad input returns `400` with a short reason, for example `MODE_INVALID` or `P_ACTIVE_INVALID`.

### `GET /reboot`

Restarts the ESP32.

## Safety

* Watchdog: if no valid `/set` arrives for `SSR_WATCHDOG_S` seconds (default 180), the firmware switches to OFF.
* DC check: if the even and odd slot counters differ, the output is blocked and the LED turns white.
* Timing check: if the SSR loop misses a slot, it logs a warning and the LED turns yellow.
* WiFi: after a few failed reconnects the WiFi stack restarts. After `WIFI_MAX_STACK_RESTARTS` restarts the ESP32 reboots.
* Temperature: not handled here. The heater thermostat and the home automation take care of that.

The API has no authentication. Only run it in a trusted network.

### Status LED

| Color | Meaning |
|---|---|
| Red | OFF |
| Green | NETZERO or NETZERO_CAP |
| Blue | SOFTZERO |
| Magenta | BURST |
| Cyan | Starting or WiFi connecting |
| Yellow | Timing error |
| White | DC imbalance, output blocked |

## Build and flash

Needs ESP-IDF 5.x (tested with 5.5.2) and an ESP32-S3.

```
idf.py set-target esp32s3
idf.py menuconfig
idf.py -p PORT flash monitor
```

Settings in `menuconfig` under Component config:

| Option | Default | Description |
|---|---|---|
| `SSR_GPIO` | 4 | GPIO for the SSR |
| `SSR_UPSHIFT_DEBOUNCE` | 2 | Messages above a threshold before stepping up |
| `SSR_WATCHDOG_S` | 180 | Seconds without a message before OFF |
| `P_BOILER_MAX_NOM` | 3400 | Heater power in W. Measure it and set the real value |
| `STATUS_LED_GPIO` | 38 | GPIO of the WS2812 status LED |
| `WIFI_SSID` | | WiFi name |
| `WIFI_PASSWORD` | | WiFi password |
| `WIFI_MAX_STACK_RESTARTS` | 3 | WiFi stack restarts before reboot |

## Hardware

* ESP32-S3 module with two cores, for example ESP32-S3-WROOM-1. A single core chip will not work because the tasks are pinned to both cores.
* Zero cross SSR for resistive loads, for example Carlo Gavazzi RGC1A23D30KKE. It should accept 3.3 V on the input and be rated well above the heater current.

Work on mains voltage only if you know what you are doing.

## Branches

* `main`: this version. Half-wave control for meters with a short integration time like the Iskra AM550.
* `generic`: the first version. Full-wave control for meters that average over 180 ms or more. It spreads the on waves with Bresenham and has a sigma-delta mode for finer levels.

## License

MIT, see [LICENSE](LICENSE).

This is a personal project. Use it at your own risk.
