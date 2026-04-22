# Rak4631-Pi2w-Backhaul-Power-System

## Why I Made This

For our Navamesh project, we need to make it as cost effective and low powered as possible just due to lack of resources available in the Navajo region. We have made soil sensor nodes that send their telemetry to a specific channel in Meshtastic, and that telemetry gets sent to our MQTT ingestor and uploaded into InfluxDB and eventually our Azure Cloud setup.

We wanted to make a backhaul/field access system that allows the farmer to get the most recent telemetry reading of his farms if he were out in the field.

<img width="1408" height="768" alt="backhbaul" src="https://github.com/user-attachments/assets/7c296b87-75ba-4d7f-96b4-e08c7bbc4d22" />


The system we made allows the farmer to send `PI_ON` in a Meshtastic chat, and the radio in the field access node that is constantly being powered (low power light sleep mode) listens for the `PI_ON`. When `PI_ON` is sent, IO1 on the RAK pulses LOW and that triggers a button press in our SparkFun power switch that allows power to feed into the Pi and turn it on.

---

# Solar Backhaul Node — Build & Deploy Guide

## What This Does

This guide walks you through building a solar-powered backhaul node that can be remotely woken up or shut down via a Meshtastic mesh message. A field user sends `PI_ON` over the mesh, the RAK radio receives it and pulses a GPIO pin, which triggers the SparkFun Soft Power Switch to boot a Raspberry Pi Zero 2W running WiFi HaLow and Reticulum backhaul.

---

## Hardware Required

| Part | Notes |
|------|-------|
| RAK19007 base board + RAK4631 core | The Meshtastic radio node |
| Raspberry Pi Zero 2W | Runs WiFi HaLow + Reticulum |
| Waveshare Solar Power Manager (MPPT) | Amazon B0G2KSZ7HX |
| SparkFun Soft Power Switch (USB-C) | SparkFun catalog |
| 12V or 18V solar panel | Connects to Waveshare solar input |
| 3.7V LiPo battery | Connects to Waveshare battery input |
| USB-A to USB-C cable | Waveshare USB-A out → SparkFun USB-C IN |
| Jumper wires | For GPIO and power connections |

---

## Wiring Diagram

```
Solar Panel
    ↓
Waveshare Solar Power Manager
    ├── USB-A out ──────────── SparkFun USB-C IN (power)
    ├── 5V pin header ──────── RAK VDD pin (power)
    └── GND pin header ─────── RAK GND pin

RAK19007 IO Header
    ├── WB_IO1 (pin 17) ────── SparkFun BTN pad
    ├── WB_IO2 (pin 34) ────── SparkFun OFF pad
    └── GND ─────────────────── SparkFun GND pad

SparkFun Soft Power Switch
    └── USB-C OUT ──────────── Pi Zero 2W (power in)
```

**GPIO Logic:**
- `WB_IO1` pulses LOW for 100ms → SparkFun sees button press → Pi boots
- `WB_IO2` pulses HIGH for 200ms then releases → SparkFun cuts output power → Pi shuts off

**Ground connections:** The Waveshare GND pin header connects to one of the RAK GND pins, and the SparkFun GND pad connects to the RAK's other GND pin. This ties all three boards to a common ground reference.

---

## Soldering

The SparkFun BTN, OFF, and GND pads are PTH (plated through-hole) breakout pads — you must solder wires to them. Friction-fit jumper wires will fail in the field.

1. Solder a wire to SparkFun **BTN** pad → connects to RAK WB_IO1
2. Solder a wire to SparkFun **OFF** pad → connects to RAK WB_IO2
3. Solder a wire to SparkFun **GND** pad → connects to RAK GND

---

## Firmware Setup

### 1. Clone the repo and switch to the backhaul branch

```bash
git clone https://github.com/Sarkors/meshtastic-soil-sensor.git
cd meshtastic-soil-sensor
git checkout backhaul
```

### 2. Confirm the right files are present

```powershell
ls src/modules/PowerTriggerModule.*
```

You should see `PowerTriggerModule.cpp` and `PowerTriggerModule.h`.

### 3. Register the module in Modules.cpp

Open `src/modules/Modules.cpp` and add the following two lines alongside the other module registrations:

```cpp
// At the top with the other includes:
#include "PowerTriggerModule.h"

// Inside the initModules() function alongside the other module instantiations:
powerTriggerModule = new PowerTriggerModule();
```

To confirm it's already there:

```powershell
Select-String -Path src/modules/Modules.cpp -Pattern "PowerTrigger"
```

Should show:

```
#include "PowerTriggerModule.h"
powerTriggerModule = new PowerTriggerModule();
```

### 4. Build and flash

```bash
pio run -e rak4631 --target clean
pio run -e rak4631
pio run -e rak4631 --target upload --upload-port COM5
```

### 5. Set the LoRa region after flashing

```bash
python -m meshtastic --port COM5 --set lora.region US
```

### 6. Confirm the module loaded

Open the serial monitor immediately after flashing:

```bash
pio device monitor --port COM5 --baud 115200
```

Look for this line within the first 10 seconds of boot:

```
PowerTrigger: ready — send PI_ON or PI_OFF — Pi forced OFF at boot
```

If you don't see it, the module isn't registered. Check Modules.cpp and reflash.

---

## How The Power Control Works

On every RAK boot the firmware forces the SparkFun output OFF by briefly asserting the OFF pin HIGH, then releasing it. This guarantees the RAK and SparkFun always start in a known synchronized state regardless of how they were powered down.

**PI_ON:** RAK pulls BTN pin LOW for 100ms (simulates a button press), then releases it HIGH. SparkFun switches its output on and the Pi boots.

**PI_OFF:** RAK drives OFF pin HIGH for 200ms (long enough for SparkFun to cut power), then releases it back LOW. The OFF pin must return to LOW so the next PI_ON BTN pulse has a clean path with no conflicting signal.

This release-after-pulse behavior is critical — leaving OFF pin HIGH after a PI_OFF would block subsequent PI_ON commands.

---

## Testing

With everything wired and firmware flashed:

1. Power on the Waveshare (press BOOT button if needed)
2. Confirm SparkFun VIN LED is on
3. Confirm Pi is OFF (SparkFun VOUT LED is off)
4. From a second Meshtastic device, send the text message **`PI_ON`**
5. Watch serial monitor — you should see:
   ```
   PowerTrigger: powering ON — pulsing BTN LOW for 100ms
   PowerTrigger: Pi ON — keepalive set for 10 mins
   ```
6. Pi should boot within a few seconds
7. Send **`PI_OFF`** — Pi should lose power
8. Send **`PI_ON`** again — Pi should boot again without needing any reset

---

## Commands Reference

| Message | Effect |
|---------|--------|
| `PI_ON` | Boots the Pi (ignored if already on, resets keepalive timer) |
| `PI_OFF` | Cuts Pi power immediately, releases OFF pin so next PI_ON works |
| `PI_RESET` | Resets internal state to OFF without touching hardware — use if PI_ON is being ignored unexpectedly |

---

## Keepalive Timer

The Pi automatically shuts off after **10 minutes** of no activity. To keep the Pi on, send `PI_ON` again before the timer expires — this resets the countdown without toggling power.

To disable the keepalive during testing, open `PowerTriggerModule.cpp` and set:

```cpp
#define KEEPALIVE_ENABLED   false
```

Set it back to `true` before field deployment.

---

## Troubleshooting

**PI_ON fires in the log but Pi doesn't boot**
- Check SparkFun VIN LED — if it's off, SparkFun isn't getting power
- Check that SparkFun GND is connected to RAK GND
- Measure voltage on SparkFun BTN pad at idle — should be ~3.3V
- During PI_ON it should briefly drop to ~0V — if it goes high instead, the firmware logic is inverted

**PI_ON ignored — Pi already on**
- The firmware state is out of sync with actual hardware state
- Send `PI_RESET` then `PI_ON`

**PI_ON doesn't work after PI_OFF**
- Make sure you're running the latest firmware where OFF pin is released back LOW after 200ms
- If using older firmware, send `PI_RESET` then `PI_ON` as a workaround

**lora rx disabled: Region unset in serial log**
- Run: `python -m meshtastic --port COM5 --set lora.region US`

**Max retransmission error from sending device**
- Check that the RAK node is online in the Meshtastic node list
- The command may still have executed — check serial log for `PowerTrigger:` lines

**SparkFun physical button doesn't work when RAK is connected**
- The RAK GPIO pins may be floating at boot before firmware initializes
- Flash the firmware — the constructor forces BTN HIGH and OFF LOW at boot which fixes this

---

## Branch Reference

| Branch | Purpose | Hardware |
|--------|---------|----------|
| `develop` | Soil sensor nodes | RAK4631 with HD-38 sensor |
| `backhaul` | Backhaul power control | RAK4631 + SparkFun + Pi Zero 2W |

Always confirm you are on the correct branch before building and flashing:

```bash
git branch          # asterisk shows current branch
git checkout backhaul   # switch to backhaul
git checkout develop    # switch to sensor nodes
```
