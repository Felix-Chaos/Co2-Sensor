#pragma once

// ============================================================================
//  Credentials (injected from .env via build flags)
//  These defaults are used if .env is missing — override via .env file
// ============================================================================
#ifndef WIFI_SSID
#define WIFI_SSID       "your_wifi_ssid"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD   "your_wifi_password"
#endif
#ifndef MQTT_SERVER
#define MQTT_SERVER     "192.168.1.100"
#endif
#ifndef MQTT_PORT
#define MQTT_PORT       1883
#endif
#ifndef MQTT_USER
#define MQTT_USER       ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD   ""
#endif

// ============================================================================
//  Feature Toggles
//  Adjust these to match your setup. Set to 0 to disable.
// ============================================================================
#define NUM_DOORS           1       // 0, 1, or 2
#define NUM_WINDOWS         2       // 0, 1, 2, or 3
#define ENABLE_STAIRS       1       // 0 = disabled, 1 = enabled
#define ENABLE_HEATING      1       // 0 = disabled, 1 = enabled
#define ENABLE_TEMP_HUM     1       // 0 = disabled, 1 = enabled (external temp/hum)
#define ENABLE_ALERTS       1       // 0 = disabled, 1 = enabled (alert overlay system)

// ============================================================================
//  MQTT Topics
// ============================================================================
#define MQTT_CLIENT_ID          "co2_sensor_ttgo"

// Publish
#define MQTT_STATE_TOPIC        "tele/co2_sensor/state"
#define MQTT_AVAILABILITY_TOPIC "tele/co2_sensor/availability"
#define MQTT_DEVICE_TOPIC       "tele/co2_sensor/device"
#define MQTT_SOURCES_TOPIC      "tele/co2_sensor/sources"

// Subscribe — data from HA (fed by HA automations)
#define MQTT_IN_TEMP            "co2_display/in/temperature"
#define MQTT_IN_HUM             "co2_display/in/humidity"
#define MQTT_IN_STAIR           "co2_display/in/stairs"
#define MQTT_IN_DOOR            "co2_display/in/door"
#define MQTT_IN_DOOR2           "co2_display/in/door2"
#define MQTT_IN_WIN1            "co2_display/in/window1"
#define MQTT_IN_WIN2            "co2_display/in/window2"
#define MQTT_IN_WIN3            "co2_display/in/window3"
#define MQTT_IN_HEAT            "co2_display/in/heating"

// Commands from HA
#define MQTT_CMD_BRIGHTNESS     "cmnd/co2_sensor/brightness"
#define MQTT_CMD_SCREEN         "cmnd/co2_sensor/screen"

// Alert toggles from HA
#define MQTT_CMD_ALERT_DOOR     "cmnd/co2_sensor/alert/door"
#define MQTT_CMD_ALERT_STAIRS   "cmnd/co2_sensor/alert/stairs"
#define MQTT_CMD_ALERT_WINDOWS  "cmnd/co2_sensor/alert/windows"
#define MQTT_CMD_ALERT_CO2      "cmnd/co2_sensor/alert/co2"
#define MQTT_CMD_ALERT_TEMPHUM  "cmnd/co2_sensor/alert/temphum"

// ============================================================================
//  Buttons (TTGO T-Display)
// ============================================================================
#define BTN_LEFT    0
#define BTN_RIGHT   35
#define NUM_SCREENS 3

// ============================================================================
//  Display & Brightness
// ============================================================================
#define TFT_BACKLIGHT_PIN       4
#define BRIGHTNESS_DAY          255
#define BRIGHTNESS_NIGHT        30
#define NIGHT_START_HOUR        22
#define NIGHT_END_HOUR          7

// ============================================================================
//  Sensor
// ============================================================================
#define SCD30_READ_INTERVAL_MS  5000
#define MQTT_PUBLISH_INTERVAL   30000
#define CO2_INVALID_READING     999

// ============================================================================
//  NTP
// ============================================================================
#define NTP_SERVER              "pool.ntp.org"
#define GMT_OFFSET_SEC          3600        // UTC+1 (CET)
#define DAYLIGHT_OFFSET_SEC     3600        // +1 for CEST (summer)

// ============================================================================
//  CO2 Thresholds (ppm)
// ============================================================================
#define CO2_GOOD                800
#define CO2_MODERATE            1000
#define CO2_POOR                1500

// ============================================================================
//  EU Comfort Ranges
// ============================================================================
#define TEMP_COMFORT_LOW    19.0f
#define TEMP_COMFORT_HIGH   23.0f
#define TEMP_WARN_LOW       16.0f
#define TEMP_WARN_HIGH      27.0f
#define HUM_COMFORT_LOW     40.0f
#define HUM_COMFORT_HIGH    60.0f
#define HUM_WARN_LOW        25.0f
#define HUM_WARN_HIGH       75.0f

// ============================================================================
//  Window Timer & Alerts
// ============================================================================
#define WIN_GOOD_MS         300000     // 5 min → thumbs up
#define WIN_CLOSE_MS        1800000    // 30 min → close windows
#define DOOR_ALERT_MS       30000      // 10 sec door notification
#define ALERT_ID_COUNT      9
