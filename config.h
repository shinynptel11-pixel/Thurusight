#pragma once

#include <Arduino.h>

// Wired pins from pin-map
static constexpr uint8_t PIN_RCWL_OUT   = 32; // IO32
static constexpr uint8_t PIN_PIR_OUT    = 33; // IO33
static constexpr uint8_t PIN_BUTTON     = 25; // IO25 (INPUT_PULLUP)
static constexpr uint8_t PIN_BUZZER     = 26; // IO26
static constexpr uint8_t PIN_I2C_SDA    = 21; // IO21
static constexpr uint8_t PIN_I2C_SCL    = 22; // IO22

static constexpr uint8_t OLED_I2C_ADDR  = 0x3C;

// Timing
static constexpr uint32_t SENSOR_SAMPLE_MS   = 100;
static constexpr uint32_t TELEMETRY_MS       = 500;
static constexpr uint32_t DISPLAY_REFRESH_MS = 500;

// Buzzer
static constexpr uint16_t BUZZER_FREQ_HZ = 2200;
