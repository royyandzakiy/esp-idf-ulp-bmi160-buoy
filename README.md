# Low-power buoy sensor (ESP32 ULP RISC-V + BMI160)

An experiment in running sensing on the ESP32's ULP RISC-V coprocessor while the main CPU stays in deep sleep. The use case is a floating buoy in a fish pond: the ULP watches water motion from a BMI160 accelerometer and only wakes the main CPU when the motion looks like fish feeding.

## How it works

```
 deep sleep ──────────────────────────────────────────────┐
   │                                                      │
   ▼                                                      │
 ULP RISC-V (main/ulp/ulp_main.c)                         │
   read BMI160 accel over ULP I2C, 8 samples              │
   |a| = integer sqrt(ax² + ay² + az²)                     │
   variance over the window, fixed-point                  │
   variance > threshold → count a "hit"                   │
   enough hits → wake main CPU ──────┐                    │
                                     ▼                    │
 Main CPU (main/main.c)                                   │
   classify(variance, hits) → CALM | FISH_ACTIVE | AGITATED
   FISH_ACTIVE → act (feed / log), reset hit counter      │
   back to deep sleep ────────────────────────────────────┘
```

- **ULP side:** fixed-point only (no FPU), Newton's-method integer square root, results stored in a struct in RTC slow memory (`ulp_shared_t`: last variance, hit count, wake reason).
- **Main CPU side:** a three-class rule-based classifier. It is deliberately simple; the point of the project is the power architecture, not the model.

## Status

Work in progress: the architecture is in place, but it has not been run with a real BMI160 yet. Next steps:

- initialise the ULP I2C peripheral and pins from the main CPU
- run the ULP on a timer instead of a GPIO wakeup, and keep the hit counter across ULP runs
- measure current draw with and without the ULP doing the sensing

## Build

Requires ESP-IDF v5.x and a chip with a ULP RISC-V coprocessor (ESP32-S2 or ESP32-S3).

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

A dev container is included in `.devcontainer/`.

## Hardware

| Part | Notes |
|---|---|
| ESP32-S2 / ESP32-S3 | needs ULP RISC-V |
| BMI160 | I2C address `0x68`, accel data read from `0x12`–`0x17` |
