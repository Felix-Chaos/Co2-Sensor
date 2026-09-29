# Home Assistant Integration Guide

This document covers how to connect the CO2 Sensor TTGO display to your Home Assistant sensors via MQTT.

## Architecture

```
┌──────────────────┐       MQTT        ┌──────────────────┐
│  Home Assistant   │ ◄──────────────► │  ESP32 TTGO      │
│                   │                   │  T-Display        │
│  Automations      │  co2_display/in/* │                   │
│  bridge entity    │ ───────────────► │  Subscribes &     │
│  states to MQTT   │                   │  renders on TFT   │
│                   │  tele/co2_sensor/ │                   │
│  MQTT Discovery   │ ◄─────────────── │  Publishes CO2    │
│  auto-registers   │                   │  & device state   │
└──────────────────┘                   └──────────────────┘
```

## Setup Steps

### 1. Configure Sources on Device Page

After flashing, the device appears in HA under **Settings → Devices → MQTT → CO2 Sensor TTGO**.

In the **Configuration** section, set each source to your HA entity ID:

| Field | Example Entity ID | Description |
|---|---|---|
| Temperature Source | `sensor.room_temperature` | Temperature sensor |
| Humidity Source | `sensor.room_humidity` | Humidity sensor |
| Door Source | `binary_sensor.front_door_contact` | Door contact sensor |
| Window 1 Source | `binary_sensor.window_left_contact` | Window 1 sensor |
| Window 2 Source | `binary_sensor.window_right_contact` | Window 2 sensor |
| Stairs Source | `binary_sensor.stair_motion_occupancy` | Stair motion sensor |
| Heating Source | `climate.thermostat` | Climate / thermostat entity |

### 2. Create the Bridge Automation

The ESP32 can't directly read HA entities — it only speaks MQTT. A single HA automation bridges your configured entities to the MQTT topics the display listens on.

**Settings → Automations → + Create Automation → ⋮ → Edit in YAML:**

```yaml
alias: "TTGO Display Bridge"
mode: restart
trigger:
  - platform: homeassistant
    event: start
    id: init
  - platform: time_pattern
    minutes: "/1"
    id: refresh
  - platform: event
    event_type: state_changed
    id: change
condition:
  - condition: template
    value_template: >
      {{ trigger.id != 'change' or trigger.event.data.entity_id in [
           states('text.co2_sensor_ttgo_temp_source'),
           states('text.co2_sensor_ttgo_humidity_source'),
           states('text.co2_sensor_ttgo_stairs_source'),
           states('text.co2_sensor_ttgo_door_source'),
           states('text.co2_sensor_ttgo_window_1_source'),
           states('text.co2_sensor_ttgo_window_2_source'),
           states('text.co2_sensor_ttgo_heating_source') ] }}
action:
  - if: [{ condition: trigger, id: init }]
    then: [{ delay: { seconds: 5 } }]
  - service: mqtt.publish
    data: { topic: "co2_display/in/temperature", payload: "{{ states(states('text.co2_sensor_ttgo_temp_source')) }}" }
  - service: mqtt.publish
    data: { topic: "co2_display/in/humidity", payload: "{{ states(states('text.co2_sensor_ttgo_humidity_source')) }}" }
  - service: mqtt.publish
    data: { topic: "co2_display/in/stairs", payload: "{{ states(states('text.co2_sensor_ttgo_stairs_source')) }}" }
  - service: mqtt.publish
    data: { topic: "co2_display/in/door", payload: "{{ states(states('text.co2_sensor_ttgo_door_source')) }}" }
  - service: mqtt.publish
    data: { topic: "co2_display/in/window1", payload: "{{ states(states('text.co2_sensor_ttgo_window_1_source')) }}" }
  - service: mqtt.publish
    data: { topic: "co2_display/in/window2", payload: "{{ states(states('text.co2_sensor_ttgo_window_2_source')) }}" }
  - service: mqtt.publish
    data: { topic: "co2_display/in/heating", payload: "{{ state_attr(states('text.co2_sensor_ttgo_heating_source'), 'hvac_action') }}" }
```

### 3. How It Works

The automation uses a `states(states(...))` pattern:
1. `states('text.co2_sensor_ttgo_door_source')` → reads the configured entity ID (e.g. `binary_sensor.front_door`)
2. `states(...)` → reads that entity's current state (e.g. `on`)
3. Publishes the result to the MQTT topic the display listens on

When you change a source on the device page, the automation automatically follows the new entity.

It listens to `state_changed` events filtered to the configured entities, so **every** transition (including `on` → `off`) is forwarded immediately. Don't use `template` triggers for this — they only fire when the template turns truthy, so a window or door closing would never be sent.

### Heating Note

For `climate.*` entities, the state (`heat`) doesn't mean actively heating. The automation uses `state_attr(..., 'hvac_action')` which returns `heating` when actively running or `idle` when not.

## MQTT Topics Reference

### Published by the device

| Topic | Content |
|---|---|
| `tele/co2_sensor/state` | `{"co2": 850}` |
| `tele/co2_sensor/device` | `{"brightness": 255, "screen": "Dashboard", "rssi": -62}` |
| `tele/co2_sensor/sources` | Configured source entity IDs |
| `tele/co2_sensor/availability` | `online` / `offline` |
| `tele/co2_sensor/event` | One-shot events, e.g. `{"event":"co2_calibrated"}` (not retained) |

### Subscribed by the device

| Topic | Expected Payload |
|---|---|
| `co2_display/in/temperature` | Number (e.g. `21.5`) |
| `co2_display/in/humidity` | Number (e.g. `55`) |
| `co2_display/in/door` | `on`/`off`, `open`/`closed` |
| `co2_display/in/window1` | `on`/`off`, `open`/`closed` |
| `co2_display/in/window2` | `on`/`off`, `open`/`closed` |
| `co2_display/in/stairs` | `on`/`off`, `detected`/`clear` |
| `co2_display/in/heating` | `heating`/`idle` |
| `cmnd/co2_sensor/brightness` | `0`-`255` |
| `cmnd/co2_sensor/screen` | `Dashboard`, `Clock`, `HA Detail` |
| `cmnd/co2_sensor/snooze` | `on`/`off` — mute all alerts for 1 hour |
| `cmnd/co2_sensor/calibrate` | any payload — forces the SCD30 to treat the current air as 400ppm |

## Physical Button Controls

The device also doubles as a Home Assistant remote: holding the buttons triggers the same actions as the `snooze`/`calibrate` topics above, and both are mirrored back as HA entities (auto-discovered):

| Button | Tap | Hold (~0.6s) |
|---|---|---|
| Left | Previous screen / dismiss alert 24h | Toggle "Snooze Alerts" switch (1h) |
| Right | Next screen / dismiss alert 7 days | Press "Calibrate CO2" button (fresh air required!) |
| Both | — | Full brightness for 20s |

**⚠️ Calibration note:** Forced recalibration tells the SCD30 the *current* reading is outdoor-fresh air (~400ppm). Only trigger it — from the device or from HA — when the sensor is actually outside or by an open window; otherwise you'll bias future readings.

## Troubleshooting

- **Entity shows "unavailable"**: Check if the entity ID is typed correctly in the source config
- **Data not updating**: Verify the automation is enabled and has recent activity in its trace
- **Duplicate device in HA**: Delete the old device, the ESP will re-register on next connect
- **Test manually**: Developer Tools → Actions → `mqtt.publish` → topic `co2_display/in/door` → payload `on`
